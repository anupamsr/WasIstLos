#include "TrayIcon.hpp"
#include <glibmm/i18n.h>
#include <gtk/gtk.h>
#include "Config.hpp"
#include <giomm/dbusintrospection.h>
#include <giomm/dbuswatchname.h>

namespace wil::ui
{
    static const char* SNI_INTERFACE_XML = R"xml(
        <!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
         "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
        <node>
            <interface name="org.kde.StatusNotifierItem">
                <property name="Category" type="s" access="read"/>
                <property name="Id" type="s" access="read"/>
                <property name="Title" type="s" access="read"/>
                <property name="Status" type="s" access="read"/>
                <property name="IconName" type="s" access="read"/>
                <property name="AttentionIconName" type="s" access="read"/>
                <property name="Menu" type="o" access="read"/>
                <method name="Activate">
                    <arg name="x" type="i" direction="in"/>
                    <arg name="y" type="i" direction="in"/>
                </method>
                <method name="SecondaryActivate">
                    <arg name="x" type="i" direction="in"/>
                    <arg name="y" type="i" direction="in"/>
                </method>
                <signal name="NewStatus">
                    <arg name="status" type="s"/>
                </signal>
                <signal name="NewIcon"/>
                <signal name="NewAttentionIcon"/>
            </interface>
        </node>
    )xml";

    static const char* DBUSMENU_INTERFACE_XML = R"xml(
        <!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
         "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
        <node>
            <interface name="com.canonical.dbusmenu">
                <property name="Version" type="u" access="read"/>
                <property name="Status" type="s" access="read"/>
                <method name="GetLayout">
                    <arg name="parentId" type="i" direction="in"/>
                    <arg name="recursionDepth" type="i" direction="in"/>
                    <arg name="propertyNames" type="as" direction="in"/>
                    <arg name="revision" type="u" direction="out"/>
                    <arg name="layout" type="(ia{sv}av)" direction="out"/>
                </method>
                <method name="GetGroupProperties">
                    <arg name="ids" type="ai" direction="in"/>
                    <arg name="propertyNames" type="as" direction="in"/>
                    <arg name="properties" type="a(ia{sv})" direction="out"/>
                </method>
                <method name="GetProperty">
                    <arg name="id" type="i" direction="in"/>
                    <arg name="name" type="s" direction="in"/>
                    <arg name="value" type="v" direction="out"/>
                </method>
                <method name="Event">
                    <arg name="id" type="i" direction="in"/>
                    <arg name="eventId" type="s" direction="in"/>
                    <arg name="data" type="v" direction="in"/>
                    <arg name="timestamp" type="u" direction="in"/>
                </method>
                <method name="EventGroup">
                    <arg name="events" type="a(isvu)" direction="in"/>
                    <arg name="idErrors" type="ai" direction="out"/>
                </method>
                <method name="AboutToShow">
                    <arg name="id" type="i" direction="in"/>
                    <arg name="needUpdate" type="b" direction="out"/>
                </method>
                <signal name="ItemsPropertiesUpdated">
                    <arg name="updatedProps" type="a(ia{sv})"/>
                    <arg name="removedProps" type="a(ias)"/>
                </signal>
                <signal name="LayoutUpdated">
                    <arg name="revision" type="u"/>
                    <arg name="parent" type="i"/>
                </signal>
            </interface>
        </node>
    )xml";

    const char* TrayIcon::findIcon(const std::initializer_list<const char*>& candidates, const char* fallback)
    {
        auto* display = gdk_display_get_default();
        if (!display) return fallback;

        auto* iconTheme = gtk_icon_theme_get_for_display(display);
        for (auto* name : candidates)
        {
            if (gtk_icon_theme_has_icon(iconTheme, name))
            {
                return name;
            }
        }
        return fallback;
    }

    const char* TrayIcon::getTrayIconName()
    {
        return findIcon({"whatsapp-tray", "com.github.eneshecan.WhatsAppForLinux-tray", WIL_ICON "-tray"}, "indicator-messages");
    }

    const char* TrayIcon::getAttentionIconName()
    {
        return findIcon({"whatsapp-msg", "com.github.eneshecan.WhatsAppForLinux-tray-attention", WIL_ICON "-tray-attention"}, "indicator-messages-new");
    }

    const char* TrayIcon::getWarningIconName()
    {
        return findIcon({"whatsapp-warning", "com.github.eneshecan.WhatsAppForLinux-tray-warning", WIL_ICON "-tray-warning"}, "network-offline");
    }

    TrayIcon::TrayIcon()
        : m_registrationId{0}
        , m_menuRegistrationId{0}
        , m_ownerId{0}
        , m_watcherWatchId{0}
        , m_menuRevision{1}
        , m_status{"Passive"}
        , m_iconName{getTrayIconName()}
        , m_attentionIconName{getAttentionIconName()}
    {
        g_message("TrayIcon: Initializing D-Bus StatusNotifierItem");

        registerOnDBus();
    }

    TrayIcon::~TrayIcon()
    {
        if (m_registrationId > 0 && m_dbusConnection)
        {
            m_dbusConnection->unregister_object(m_registrationId);
        }

        if (m_menuRegistrationId > 0 && m_dbusConnection)
        {
            m_dbusConnection->unregister_object(m_menuRegistrationId);
        }

        if (m_ownerId > 0)
        {
            Gio::DBus::unown_name(m_ownerId);
        }

        if (m_watcherWatchId > 0)
        {
            Gio::DBus::unwatch_name(m_watcherWatchId);
        }
    }

    void TrayIcon::registerOnDBus()
    {
        try
        {
            m_dbusConnection = Gio::DBus::Connection::get_sync(Gio::DBus::BusType::SESSION);

            if (!m_dbusConnection)
            {
                g_warning("TrayIcon: Failed to get D-Bus session connection");
                return;
            }

            g_message("TrayIcon: Got D-Bus session connection");

            auto nodeInfo = Gio::DBus::NodeInfo::create_for_xml(SNI_INTERFACE_XML);
            m_introspection = nodeInfo->lookup_interface("org.kde.StatusNotifierItem");

            if (!m_introspection)
            {
                g_warning("TrayIcon: Failed to parse StatusNotifierItem interface");
                return;
            }

            m_vtable = std::make_unique<Gio::DBus::InterfaceVTable>(
                sigc::mem_fun(*this, &TrayIcon::onMethodCall),
                sigc::mem_fun(*this, &TrayIcon::onPropertyGet)
            );

            m_registrationId = m_dbusConnection->register_object(
                "/StatusNotifierItem",
                m_introspection,
                *m_vtable
            );

            g_message("TrayIcon: Registered StatusNotifierItem object (id=%u)", m_registrationId);

            auto menuNodeInfo = Gio::DBus::NodeInfo::create_for_xml(DBUSMENU_INTERFACE_XML);
            m_menuIntrospection = menuNodeInfo->lookup_interface("com.canonical.dbusmenu");

            if (!m_menuIntrospection)
            {
                g_warning("TrayIcon: Failed to parse dbusmenu interface");
                return;
            }

            m_menuVtable = std::make_unique<Gio::DBus::InterfaceVTable>(
                sigc::mem_fun(*this, &TrayIcon::onMenuMethodCall),
                sigc::mem_fun(*this, &TrayIcon::onMenuPropertyGet)
            );

            m_menuRegistrationId = m_dbusConnection->register_object(
                "/MenuBar",
                m_menuIntrospection,
                *m_menuVtable
            );

            g_message("TrayIcon: Registered dbusmenu object (id=%u)", m_menuRegistrationId);

            m_watcherWatchId = Gio::DBus::watch_name(
                m_dbusConnection,
                "org.kde.StatusNotifierWatcher",
                sigc::mem_fun(*this, &TrayIcon::onWatcherAppeared),
                {}
            );
        }
        catch (const Glib::Error& error)
        {
            g_warning("TrayIcon: D-Bus registration failed: %s", error.what());
        }
    }

    void TrayIcon::onWatcherAppeared(const Glib::RefPtr<Gio::DBus::Connection>& connection, const Glib::ustring& name, const Glib::ustring& nameOwner)
    {
        try
        {
            auto watcher = Gio::DBus::Proxy::create_sync(
                connection,
                "org.kde.StatusNotifierWatcher",
                "/StatusNotifierWatcher",
                "org.kde.StatusNotifierWatcher"
            );

            if (!watcher)
            {
                g_warning("TrayIcon: Could not connect to StatusNotifierWatcher");
                return;
            }

            auto busName = connection->get_unique_name();
            auto params = Glib::VariantContainerBase::create_tuple({
                Glib::Variant<Glib::ustring>::create(busName)
            });

            watcher->call_sync("RegisterStatusNotifierItem", params);
            g_message("TrayIcon: Registered with StatusNotifierWatcher as %s", busName.c_str());
        }
        catch (const Glib::Error& error)
        {
            g_warning("TrayIcon: Failed to register with StatusNotifierWatcher: %s", error.what());
        }
    }

    void TrayIcon::onMethodCall(
        const Glib::RefPtr<Gio::DBus::Connection>& connection,
        const Glib::ustring& sender,
        const Glib::ustring& object_path,
        const Glib::ustring& interface_name,
        const Glib::ustring& method_name,
        const Glib::VariantContainerBase& parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation)
    {
        g_message("TrayIcon: Method call: %s", method_name.c_str());

        if (method_name == "Activate")
        {
            m_signalShow.emit();
            invocation->return_value({});
        }
        else if (method_name == "SecondaryActivate")
        {
            invocation->return_value({});
        }
        else
        {
            invocation->return_error(Gio::DBus::Error(Gio::DBus::Error::UNKNOWN_METHOD, "Method not implemented"));
        }
    }

    void TrayIcon::onPropertyGet(
        Glib::VariantBase& property,
        const Glib::RefPtr<Gio::DBus::Connection>& connection,
        const Glib::ustring& sender,
        const Glib::ustring& object_path,
        const Glib::ustring& interface_name,
        const Glib::ustring& property_name)
    {
        g_message("TrayIcon: Property get: %s", property_name.c_str());

        if (property_name == "Category")
        {
            property = Glib::Variant<Glib::ustring>::create("Communications");
        }
        else if (property_name == "Id")
        {
            property = Glib::Variant<Glib::ustring>::create(WIL_APP_ID);
        }
        else if (property_name == "Title")
        {
            property = Glib::Variant<Glib::ustring>::create(WIL_FRIENDLY_NAME);
        }
        else if (property_name == "Status")
        {
            property = Glib::Variant<Glib::ustring>::create(m_status);
        }
        else if (property_name == "IconName")
        {
            property = Glib::Variant<Glib::ustring>::create(m_iconName);
        }
        else if (property_name == "AttentionIconName")
        {
            property = Glib::Variant<Glib::ustring>::create(m_attentionIconName);
        }
        else if (property_name == "Menu")
        {
            property = Glib::Variant<Glib::DBusObjectPathString>::create("/MenuBar");
        }
    }

    namespace
    {
        GVariant* buildMenuItem(int id, const char* label)
        {
            GVariantBuilder props;
            g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string(label));
            g_variant_builder_add(&props, "{sv}", "enabled", g_variant_new_boolean(TRUE));
            g_variant_builder_add(&props, "{sv}", "visible", g_variant_new_boolean(TRUE));

            GVariantBuilder children;
            g_variant_builder_init(&children, G_VARIANT_TYPE("av"));

            return g_variant_new("(ia{sv}av)", id, &props, &children);
        }

        void appendItemProperties(GVariantBuilder* arr, int id, const char* label)
        {
            GVariantBuilder props;
            g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&props, "{sv}", "label", g_variant_new_string(label));
            g_variant_builder_add(&props, "{sv}", "enabled", g_variant_new_boolean(TRUE));
            g_variant_builder_add(&props, "{sv}", "visible", g_variant_new_boolean(TRUE));
            g_variant_builder_add(arr, "(ia{sv})", id, &props);
        }
    }

    void TrayIcon::dispatchMenuItem(int id)
    {
        switch (id)
        {
            case 1:
                m_signalShow.emit();
                break;
            case 2:
                m_signalAbout.emit();
                break;
            case 3:
                m_signalQuit.emit();
                break;
            default:
                break;
        }
    }

    void TrayIcon::onMenuMethodCall(
        const Glib::RefPtr<Gio::DBus::Connection>& connection,
        const Glib::ustring& sender,
        const Glib::ustring& object_path,
        const Glib::ustring& interface_name,
        const Glib::ustring& method_name,
        const Glib::VariantContainerBase& parameters,
        const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation)
    {
        g_message("TrayIcon: Menu method call: %s", method_name.c_str());

        if (method_name == "GetLayout")
        {
            GVariantBuilder rootChildren;
            g_variant_builder_init(&rootChildren, G_VARIANT_TYPE("av"));
            g_variant_builder_add(&rootChildren, "v", buildMenuItem(1, _("Show")));
            g_variant_builder_add(&rootChildren, "v", buildMenuItem(2, _("About")));
            g_variant_builder_add(&rootChildren, "v", buildMenuItem(3, _("Quit")));

            GVariantBuilder rootProps;
            g_variant_builder_init(&rootProps, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&rootProps, "{sv}", "children-display", g_variant_new_string("submenu"));

            GVariant* ret = g_variant_new("(u(ia{sv}av))", m_menuRevision, 0, &rootProps, &rootChildren);
            invocation->return_value(Glib::VariantContainerBase(ret));
        }
        else if (method_name == "GetGroupProperties")
        {
            GVariantBuilder arr;
            g_variant_builder_init(&arr, G_VARIANT_TYPE("a(ia{sv})"));
            appendItemProperties(&arr, 1, _("Show"));
            appendItemProperties(&arr, 2, _("About"));
            appendItemProperties(&arr, 3, _("Quit"));

            GVariant* ret = g_variant_new("(a(ia{sv}))", &arr);
            invocation->return_value(Glib::VariantContainerBase(ret));
        }
        else if (method_name == "GetProperty")
        {
            gint32 id = 0;
            const gchar* name = nullptr;
            GVariant* params = const_cast<GVariant*>(parameters.gobj());
            g_variant_get_child(params, 0, "i", &id);
            g_variant_get_child(params, 1, "&s", &name);

            const char* label = id == 1 ? _("Show") : id == 2 ? _("About") : id == 3 ? _("Quit") : "";
            GVariant* value = g_str_equal(name, "label") ? g_variant_new_string(label) : g_variant_new_boolean(TRUE);

            GVariant* ret = g_variant_new("(v)", value);
            invocation->return_value(Glib::VariantContainerBase(ret));
        }
        else if (method_name == "Event")
        {
            gint32 id = 0;
            const gchar* eventId = nullptr;
            GVariant* params = const_cast<GVariant*>(parameters.gobj());
            g_variant_get_child(params, 0, "i", &id);
            g_variant_get_child(params, 1, "&s", &eventId);

            if (g_str_equal(eventId, "clicked"))
            {
                dispatchMenuItem(id);
            }
            invocation->return_value({});
        }
        else if (method_name == "EventGroup")
        {
            GVariantBuilder idErrors;
            g_variant_builder_init(&idErrors, G_VARIANT_TYPE("ai"));
            GVariant* ret = g_variant_new("(ai)", &idErrors);
            invocation->return_value(Glib::VariantContainerBase(ret));
        }
        else if (method_name == "AboutToShow")
        {
            GVariant* ret = g_variant_new("(b)", FALSE);
            invocation->return_value(Glib::VariantContainerBase(ret));
        }
        else
        {
            invocation->return_error(Gio::DBus::Error(Gio::DBus::Error::UNKNOWN_METHOD, "Method not implemented"));
        }
    }

    void TrayIcon::onMenuPropertyGet(
        Glib::VariantBase& property,
        const Glib::RefPtr<Gio::DBus::Connection>& connection,
        const Glib::ustring& sender,
        const Glib::ustring& object_path,
        const Glib::ustring& interface_name,
        const Glib::ustring& property_name)
    {
        if (property_name == "Version")
        {
            property = Glib::Variant<guint32>::create(3);
        }
        else if (property_name == "Status")
        {
            property = Glib::Variant<Glib::ustring>::create("normal");
        }
    }

    void TrayIcon::setVisible(bool visible)
    {
        g_message("TrayIcon::setVisible(%s)", visible ? "true" : "false");
        m_status = visible ? "Active" : "Passive";

        if (m_dbusConnection && m_registrationId > 0)
        {
            try
            {
                auto params = Glib::VariantContainerBase::create_tuple({
                    Glib::Variant<Glib::ustring>::create(m_status)
                });

                m_dbusConnection->emit_signal(
                    "/StatusNotifierItem",
                    "org.kde.StatusNotifierItem",
                    "NewStatus",
                    {},
                    params
                );

                g_message("TrayIcon: Emitted NewStatus signal: %s", m_status.c_str());
            }
            catch (const Glib::Error& error)
            {
                g_warning("TrayIcon: Failed to emit NewStatus: %s", error.what());
            }
        }
    }

    bool TrayIcon::isVisible() const
    {
        return m_status != "Passive";
    }

    void TrayIcon::setAttention(bool attention)
    {
        if (!isVisible())
        {
            return;
        }

        g_message("TrayIcon::setAttention(%s)", attention ? "true" : "false");

        m_attentionIconName = getAttentionIconName();
        m_status = attention ? "NeedsAttention" : "Active";

        if (m_dbusConnection && m_registrationId > 0)
        {
            try
            {
                m_dbusConnection->emit_signal(
                    "/StatusNotifierItem",
                    "org.kde.StatusNotifierItem",
                    "NewAttentionIcon",
                    {}
                );

                auto params = Glib::VariantContainerBase::create_tuple({
                    Glib::Variant<Glib::ustring>::create(m_status)
                });

                m_dbusConnection->emit_signal(
                    "/StatusNotifierItem",
                    "org.kde.StatusNotifierItem",
                    "NewStatus",
                    {},
                    params
                );

                g_message("TrayIcon: Set attention mode with message icon");
            }
            catch (const Glib::Error& error)
            {
                g_warning("TrayIcon: Failed to set attention: %s", error.what());
            }
        }
    }

    void TrayIcon::setWarning(bool warning)
    {
        if (!isVisible())
        {
            return;
        }

        g_message("TrayIcon::setWarning(%s)", warning ? "true" : "false");

        m_attentionIconName = getWarningIconName();
        m_status = warning ? "NeedsAttention" : "Active";

        if (m_dbusConnection && m_registrationId > 0)
        {
            try
            {
                m_dbusConnection->emit_signal(
                    "/StatusNotifierItem",
                    "org.kde.StatusNotifierItem",
                    "NewAttentionIcon",
                    {}
                );

                auto params = Glib::VariantContainerBase::create_tuple({
                    Glib::Variant<Glib::ustring>::create(m_status)
                });

                m_dbusConnection->emit_signal(
                    "/StatusNotifierItem",
                    "org.kde.StatusNotifierItem",
                    "NewStatus",
                    {},
                    params
                );

                g_message("TrayIcon: Set warning mode with warning icon");
            }
            catch (const Glib::Error& error)
            {
                g_warning("TrayIcon: Failed to set warning: %s", error.what());
            }
        }
    }

    sigc::signal<void()>& TrayIcon::signalShow() noexcept
    {
        return m_signalShow;
    }

    sigc::signal<void()>& TrayIcon::signalAbout() noexcept
    {
        return m_signalAbout;
    }

    sigc::signal<void()>& TrayIcon::signalQuit() noexcept
    {
        return m_signalQuit;
    }
}
