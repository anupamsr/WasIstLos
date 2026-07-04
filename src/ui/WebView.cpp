#include "WebView.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <optional>
#include <locale>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gtkmm/alertdialog.h>
#include <gtk/gtk.h>
#include "../util/Settings.hpp"
#include "Config.hpp"

namespace wil::ui
{
    namespace
    {
        constexpr auto const WHATSAPP_WEB_URI = "https://web.whatsapp.com";
        constexpr auto const USER_AGENT       = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

        std::optional<std::string> getSystemLanguage()
        {
            try
            {
                auto const lang = std::locale{""}.name();
                return lang.substr(0, lang.find('.'));
            }
            catch (std::runtime_error const& error)
            {
                std::cerr << "WebView: Failed to get system language: " << error.what() << std::endl;
                return std::nullopt;
            }
        }

        gboolean permissionRequest(WebKitWebView*, WebKitPermissionRequest* request, GtkWindow*)
        {
            if (util::Settings::getInstance().getValue<bool>("web", "allow-permissions"))
            {
                webkit_permission_request_allow(request);
                return TRUE;
            }

            // TODO: GTK4 - Convert to async Gtk::AlertDialog pattern
            // For now, auto-allow on first request (user can disable in preferences)
            webkit_permission_request_allow(request);
            util::Settings::getInstance().setValue("web", "allow-permissions", true);

            return TRUE;
        }

        gboolean decidePolicy(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType decisionType, gpointer)
        {
            switch (decisionType)
            {
                case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
                {
                    auto const navigationDecision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
                    auto const navigationAction   = webkit_navigation_policy_decision_get_navigation_action(navigationDecision);
                    auto const request            = webkit_navigation_action_get_request(navigationAction);
                    auto const uri                = webkit_uri_request_get_uri(request);

                    // GTK4: gtk_show_uri (no GError return, uses async internally)
                    gtk_show_uri(nullptr, uri, GDK_CURRENT_TIME);

                    return TRUE;
                }

                default:
                    return FALSE;
            }
        }

        gboolean downloadDecideDestination(WebKitDownload* download, char* suggestedFilename, gpointer)
        {
            // TODO: GTK4 - Convert to async GtkFileDialog pattern
            // For now, save to ~/Downloads with the suggested filename
            auto const downloadsDir = Glib::get_user_special_dir(Glib::UserDirectory::DOWNLOAD);
            auto const destination  = "file://" + downloadsDir + "/" + std::string(suggestedFilename);
            webkit_download_set_destination(download, destination.c_str());

            std::cout << "WebView: Downloading to " << destination << std::endl;

            return TRUE;
        }

        void downloadStarted(WebKitWebContext*, WebKitDownload* download, gpointer)
        {
            g_signal_connect(download, "decide-destination", G_CALLBACK(downloadDecideDestination), nullptr);
        }

        void initializeNotificationPermission(WebKitWebContext* context, gpointer)
        {
            // TODO: GTK4/webkitgtk-6.0 - Check if this API still exists or moved to WebKitNetworkSession
            // webkit_web_context_initialize_notification_permissions may have been removed/changed
            // Notifications should still work via the permission-request signal
            if (util::Settings::getInstance().getValue<bool>("web", "allow-permissions"))
            {
                auto const origin         = webkit_security_origin_new_for_uri(WHATSAPP_WEB_URI);
                auto const allowedOrigins = g_list_alloc();
                allowedOrigins->data      = origin;

                // webkit_web_context_initialize_notification_permissions(context, allowedOrigins, nullptr);

                g_list_free(allowedOrigins);
                webkit_security_origin_unref(origin);
            }
        }

        bool cssFileExists(const std::string& filePath)
        {
            auto const file = std::ifstream(filePath);
            return file.good();
        }

        std::string loadCssContent(const std::string& cssFilePath)
        {
            auto cssFile    = std::ifstream(cssFilePath);
            auto cssContent = std::string((std::istreambuf_iterator<char>(cssFile)), std::istreambuf_iterator<char>());

            return cssContent;
        }
    }

    namespace detail
    {
        void loadChanged(WebKitWebView*, WebKitLoadEvent loadEvent, gpointer userData)
        {
            if (auto const webView = reinterpret_cast<WebView*>(userData); webView)
            {
                webView->onLoadStatusChanged(loadEvent);
            }
        }

        gboolean loadFailed(WebKitWebView*, WebKitLoadEvent, char*, GError*, gpointer userData)
        {
            if (auto const webView = reinterpret_cast<WebView*>(userData); webView)
            {
                webView->signalConnectivity().emit(false);
            }
            return FALSE;
        }

        void notificationDestroyed(WebKitNotification*, gpointer userData)
        {
            if (auto const webView = reinterpret_cast<WebView*>(userData); webView)
            {
                webView->signalNotification().emit(false);
            }
        }

        void notificationClicked(WebKitNotification*, gpointer userData)
        {
            if (auto const webView = reinterpret_cast<WebView*>(userData); webView)
            {
                webView->signalNotificationClicked().emit();
            }
        }

        gboolean showNotification(WebKitWebView*, WebKitNotification* notification, gpointer userData)
        {
            auto const webView = reinterpret_cast<WebView*>(userData);
            if (webView)
            {
                webView->signalNotification().emit(true);
            }

            g_signal_connect(notification, "clicked", G_CALLBACK(notificationClicked), webView);
            g_signal_connect(notification, "closed", G_CALLBACK(notificationDestroyed), webView);

            return FALSE;
        }
    }


    WebView::WebView()
        : Gtk::Widget{webkit_web_view_new()}
        , m_loadStatus{WEBKIT_LOAD_STARTED}
        , m_stoppedResponding{false}
        , m_signalLoadStatus{}
        , m_signalNotification{}
        , m_signalNotificationClicked{}
        , m_signalConnectivity{}
    {
        auto const webContext = webkit_web_view_get_context(*this);

        auto configDir   = Glib::get_user_config_dir();
        auto cssFilePath = configDir + "/" + WIL_NAME + "/web.css";

        g_signal_connect(*this, "load-changed", G_CALLBACK(detail::loadChanged), this);
        g_signal_connect(*this, "load-failed", G_CALLBACK(detail::loadFailed), this);
        g_signal_connect(*this, "permission-request", G_CALLBACK(permissionRequest), nullptr);
        g_signal_connect(*this, "decide-policy", G_CALLBACK(decidePolicy), nullptr);
        g_signal_connect(*this, "show-notification", G_CALLBACK(detail::showNotification), this);
        // download-started signal removed in WebKitGTK 6.0
        // g_signal_connect(webContext, "download-started", G_CALLBACK(downloadStarted), nullptr);
        g_signal_connect(webContext, "initialize-notification-permissions", G_CALLBACK(initializeNotificationPermission), nullptr);
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &WebView::onTimeout), 5000);

        if (auto const lang = getSystemLanguage(); lang.has_value())
        {
            gchar const* const spellCheckingLangs[] = {lang.value().c_str(), nullptr};
            webkit_web_context_set_spell_checking_enabled(webContext, TRUE);
            webkit_web_context_set_spell_checking_languages(webContext, spellCheckingLangs);
        }

        auto const settings = webkit_web_view_get_settings(*this);
        webkit_settings_set_user_agent(settings, USER_AGENT);
        webkit_settings_set_enable_developer_extras(settings, TRUE);
        auto hwAccelPolicy = static_cast<WebKitHardwareAccelerationPolicy>(util::Settings::getInstance().getValue<int>("web", "hw-accel", 1));
        webkit_settings_set_hardware_acceleration_policy(settings, hwAccelPolicy);
        webkit_settings_set_minimum_font_size(settings, util::Settings::getInstance().getValue<int>("web", "min-font-size", 0));

        webkit_web_view_set_zoom_level(*this, util::Settings::getInstance().getValue<double>("general", "zoom-level", 1.0));

        if (cssFileExists(cssFilePath))
        {
            applyCustomCss(cssFilePath);
        }

        webkit_web_view_load_uri(*this, WHATSAPP_WEB_URI);
    }

    WebView::~WebView()
    {
        webkit_web_view_terminate_web_process(*this);
    }

    WebView::operator WebKitWebView*()
    {
        return WEBKIT_WEB_VIEW(gobj());
    }

    void WebView::refresh()
    {
        webkit_web_view_reload(*this);
    }

    WebKitLoadEvent WebView::getLoadStatus() const noexcept
    {
        return m_loadStatus;
    }

    void WebView::setHwAccelPolicy(WebKitHardwareAccelerationPolicy policy)
    {
        auto const settings = webkit_web_view_get_settings(*this);
        webkit_settings_set_hardware_acceleration_policy(settings, policy);
    }

    void WebView::sendRequest(std::string url)
    {
        if (auto const uriPrefix = std::string{"whatsapp:/"}; url.find(uriPrefix) != std::string::npos)
        {
            url.replace(0U, uriPrefix.size(), WHATSAPP_WEB_URI);

            std::cerr << "WebView: Sending request: " << url << std::endl;

            auto script = std::string{};
            script.append("(function(){"
                          "var a = document.createElement(\"a\");"
                          "a.href = \"");
            script.append(url);
            script.append("\";"
                          "document.body.appendChild(a);"
                          "a.click();"
                          "a.remove();"
                          "})();");

            webkit_web_view_evaluate_javascript(*this, script.c_str(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
        else
        {
            std::cerr << "WebView: Invalid url: " << url << std::endl;
        }
    }

    void WebView::zoomIn()
    {
        if (auto zoomLevel = webkit_web_view_get_zoom_level(*this); zoomLevel < 2.0)
        {
            zoomLevel += 0.05;
            webkit_web_view_set_zoom_level(*this, zoomLevel);
            util::Settings::getInstance().setValue("general", "zoom-level", zoomLevel);
        }
    }

    void WebView::zoomOut()
    {
        if (auto zoomLevel = webkit_web_view_get_zoom_level(*this); zoomLevel > 0.5)
        {
            zoomLevel -= 0.05;
            webkit_web_view_set_zoom_level(*this, zoomLevel);
            util::Settings::getInstance().setValue("general", "zoom-level", zoomLevel);
        }
    }

    void WebView::resetZoom()
    {
        auto const defaultLevel = 1.0;
        webkit_web_view_set_zoom_level(*this, defaultLevel);
        util::Settings::getInstance().setValue("general", "zoom-level", defaultLevel);
    }


    double WebView::getZoomLevel()
    {
        return webkit_web_view_get_zoom_level(*this);
    }

    std::string WebView::getZoomLevelString()
    {
        return std::to_string(static_cast<int>(std::round(getZoomLevel() * 100))).append("%");
    }

    void WebView::setMinFontSize(unsigned int fontSize)
    {
        auto const settings = webkit_web_view_get_settings(*this);
        webkit_settings_set_minimum_font_size(settings, fontSize);
    }

    sigc::signal<void(WebKitLoadEvent)>& WebView::signalLoadStatus() noexcept
    {
        return m_signalLoadStatus;
    }

    sigc::signal<void(bool)>& WebView::signalNotification() noexcept
    {
        return m_signalNotification;
    }

    sigc::signal<void()>& WebView::signalNotificationClicked() noexcept
    {
        return m_signalNotificationClicked;
    }

    sigc::signal<void(bool)>& WebView::signalConnectivity() noexcept
    {
        return m_signalConnectivity;
    }

    void WebView::onLoadStatusChanged(WebKitLoadEvent loadEvent)
    {
        m_loadStatus = loadEvent;
        m_signalLoadStatus.emit(m_loadStatus);

        if (loadEvent == WEBKIT_LOAD_FINISHED)
        {
            m_signalConnectivity.emit(true);
        }
    }

    bool WebView::onTimeout()
    {
        auto const responsive = webkit_web_view_get_is_web_process_responsive(*this);
        // Give a second chance to WebView for recovering itself by checking if it stopped responding before
        if (!responsive && m_stoppedResponding)
        {
            // TODO: GTK4 - Convert to async Gtk::AlertDialog pattern
            // For now, auto-reload on unresponsive state
            std::cerr << "WebView: Unresponsive, auto-reloading" << std::endl;
            webkit_web_view_terminate_web_process(*this);
            webkit_web_view_reload(*this);
        }
        m_stoppedResponding = !responsive;

        return true;
    }

    void WebView::applyCustomCss(const std::string& cssFilePath)
    {
        auto const cssContent = loadCssContent(cssFilePath);

        auto* styleSheet
            = webkit_user_style_sheet_new(cssContent.c_str(), WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES, WEBKIT_USER_STYLE_LEVEL_USER, nullptr, nullptr);

        auto* manager = webkit_web_view_get_user_content_manager(*this);
        webkit_user_content_manager_add_style_sheet(manager, styleSheet);
    }
}
