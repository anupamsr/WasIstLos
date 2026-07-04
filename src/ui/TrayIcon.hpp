#pragma once

#include <memory>
#include <sigc++/signal.h>
#include <gtkmm.h>
#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>

namespace wil::ui
{
    class TrayIcon
    {
        public:
            TrayIcon();
            ~TrayIcon();

        public:
            void setVisible(bool visible);
            bool isVisible() const;
            void setAttention(bool attention);
            void setWarning(bool warning);

        public:
            sigc::signal<void()>& signalShow() noexcept;
            sigc::signal<void()>& signalAbout() noexcept;
            sigc::signal<void()>& signalQuit() noexcept;

        private:
            void registerOnDBus();
            void onWatcherAppeared(const Glib::RefPtr<Gio::DBus::Connection>& connection, const Glib::ustring& name, const Glib::ustring& nameOwner);
            void onBusAcquired(const Glib::RefPtr<Gio::DBus::Connection>& connection);
            void onMethodCall(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                            const Glib::ustring& sender,
                            const Glib::ustring& object_path,
                            const Glib::ustring& interface_name,
                            const Glib::ustring& method_name,
                            const Glib::VariantContainerBase& parameters,
                            const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
            void onPropertyGet(Glib::VariantBase& property,
                             const Glib::RefPtr<Gio::DBus::Connection>& connection,
                             const Glib::ustring& sender,
                             const Glib::ustring& object_path,
                             const Glib::ustring& interface_name,
                             const Glib::ustring& property_name);
            void onMenuMethodCall(const Glib::RefPtr<Gio::DBus::Connection>& connection,
                            const Glib::ustring& sender,
                            const Glib::ustring& object_path,
                            const Glib::ustring& interface_name,
                            const Glib::ustring& method_name,
                            const Glib::VariantContainerBase& parameters,
                            const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation);
            void onMenuPropertyGet(Glib::VariantBase& property,
                             const Glib::RefPtr<Gio::DBus::Connection>& connection,
                             const Glib::ustring& sender,
                             const Glib::ustring& object_path,
                             const Glib::ustring& interface_name,
                             const Glib::ustring& property_name);
            void dispatchMenuItem(int id);

            const char* findIcon(const std::initializer_list<const char*>& candidates, const char* fallback);
            const char* getTrayIconName();
            const char* getAttentionIconName();
            const char* getWarningIconName();

        private:
            Glib::RefPtr<Gio::DBus::Connection>        m_dbusConnection;
            Glib::RefPtr<Gio::DBus::InterfaceInfo>     m_introspection;
            std::unique_ptr<Gio::DBus::InterfaceVTable> m_vtable;
            Glib::RefPtr<Gio::DBus::InterfaceInfo>     m_menuIntrospection;
            std::unique_ptr<Gio::DBus::InterfaceVTable> m_menuVtable;
            guint                               m_registrationId;
            guint                               m_menuRegistrationId;
            guint                               m_ownerId;
            guint                               m_watcherWatchId;
            guint                               m_menuRevision;

            Glib::ustring                       m_status;          // "Passive", "Active", "NeedsAttention"
            Glib::ustring                       m_iconName;
            Glib::ustring                       m_attentionIconName;

            sigc::signal<void()> m_signalShow;
            sigc::signal<void()> m_signalAbout;
            sigc::signal<void()> m_signalQuit;
    };
}
