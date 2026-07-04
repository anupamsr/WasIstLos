#include "MainWindow.hpp"
#include <glibmm/i18n.h>
#include <gtkmm/grid.h>
#include <gtkmm/button.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/shortcutswindow.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollerscroll.h>
#include "Application.hpp"
#include "Config.hpp"
#include "../util/Settings.hpp"

namespace wil::ui
{
    MainWindow::MainWindow(BaseObjectType* cobject, Glib::RefPtr<Gtk::Builder> const& refBuilder)
        : Gtk::ApplicationWindow{cobject}
        , m_trayIcon{}
        , m_webView{}
        , m_pendingUrl{}
        , m_preferencesWindow{nullptr}
        , m_headerBar{nullptr}
        , m_shortcutsWindow{nullptr}
        , m_aboutDialog{nullptr}
        , m_buttonZoomLevel{nullptr}
    {
        set_icon_name(WIL_ICON);

        auto gridMain = refBuilder->get_widget<Gtk::Grid>("grid_main");
        gridMain->attach(m_webView, 0, 0);


        auto buttonRefresh = refBuilder->get_widget<Gtk::Button>("button_refresh");
        buttonRefresh->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onRefresh));

        m_headerBar = refBuilder->get_widget<Gtk::HeaderBar>("headerbar");

        auto buttonFullscreen = refBuilder->get_widget<Gtk::Button>("button_fullscreen");
        buttonFullscreen->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onFullscreen));

        m_buttonZoomLevel = refBuilder->get_widget<Gtk::Button>("button_zoom_level");
        m_buttonZoomLevel->set_label(m_webView.getZoomLevelString());
        m_buttonZoomLevel->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onResetZoom));

        auto buttonZoomIn = refBuilder->get_widget<Gtk::Button>("button_zoom_in");
        buttonZoomIn->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onZoomIn));

        auto buttonZoomOut = refBuilder->get_widget<Gtk::Button>("button_zoom_out");
        buttonZoomOut->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onZoomOut));

        auto buttonPreferences = refBuilder->get_widget<Gtk::Button>("button_preferences");
        buttonPreferences->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onOpenPreferences));

        auto buttonShortcuts = refBuilder->get_widget<Gtk::Button>("button_shortcuts");
        buttonShortcuts->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onShortcuts));

        auto buttonAbout = refBuilder->get_widget<Gtk::Button>("button_about");
        buttonAbout->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onAbout));

        auto buttonQuit = refBuilder->get_widget<Gtk::Button>("button_quit");
        buttonQuit->signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onQuit));

        m_webView.signalLoadStatus().connect(sigc::mem_fun(*this, &MainWindow::onLoadStatusChanged));
        m_webView.signalNotification().connect(sigc::mem_fun(*this, &MainWindow::onNotificationChanged));
        m_webView.signalNotificationClicked().connect(sigc::mem_fun(*this, &MainWindow::onShow));
        m_webView.signalConnectivity().connect(sigc::mem_fun(*this, &MainWindow::onConnectivityChanged));
        m_trayIcon.signalShow().connect(sigc::mem_fun(*this, &MainWindow::onShow));
        m_trayIcon.signalAbout().connect(sigc::mem_fun(*this, &MainWindow::onAbout));
        m_trayIcon.signalQuit().connect(sigc::mem_fun(*this, &MainWindow::onQuit));

        auto keyController = Gtk::EventControllerKey::create();
        keyController->signal_key_pressed().connect(sigc::mem_fun(*this, &MainWindow::onKeyPressed), false);
        add_controller(keyController);

        auto scrollController = Gtk::EventControllerScroll::create();
        scrollController->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        scrollController->signal_scroll().connect(sigc::mem_fun(*this, &MainWindow::onScroll), false);
        add_controller(scrollController);

        signal_close_request().connect(sigc::mem_fun(*this, &MainWindow::onCloseRequest), false);

        m_trayIcon.setVisible(util::Settings::getInstance().getValue<bool>("general", "close-to-tray"));
        m_headerBar->set_visible(util::Settings::getInstance().getValue<bool>("general", "header-bar", true));
    }

    void MainWindow::openUrl(std::string const& url)
    {
        if (m_webView.getLoadStatus() == WEBKIT_LOAD_FINISHED)
        {
            m_webView.sendRequest(url);
        }
        else
        {
            m_pendingUrl = url;
        }
    }

    bool MainWindow::onKeyPressed(guint keyval, guint keycode, Gdk::ModifierType state)
    {
        if ((state & Gdk::ModifierType::CONTROL_MASK) != Gdk::ModifierType(0))
        {
            switch (keyval)
            {
                case GDK_KEY_P:
                case GDK_KEY_p:
                    onOpenPreferences();
                    return true;

                case GDK_KEY_Q:
                case GDK_KEY_q:
                    onQuit();
                    return true;

                case GDK_KEY_question:
                    onShortcuts();
                    return true;

                case GDK_KEY_plus:
                    onZoomIn();
                    return true;

                case GDK_KEY_minus:
                    onZoomOut();
                    return true;

                default:
                    break;
            }
        }
        else if ((state & Gdk::ModifierType::ALT_MASK) != Gdk::ModifierType(0))
        {
            switch (keyval)
            {
                case GDK_KEY_H:
                case GDK_KEY_h:
                {
                    auto const visible = !m_headerBar->is_visible();
                    m_headerBar->set_visible(visible);
                    util::Settings::getInstance().setValue("general", "header-bar", visible);
                    return true;
                }

                default:
                    break;
            }
        }
        else
        {
            switch (keyval)
            {
                case GDK_KEY_F11:
                    onFullscreen();
                    return true;

                default:
                    break;
            }
        }

        return false;
    }

    bool MainWindow::onScroll(double dx, double dy)
    {
        if (dy < 0)
        {
            onZoomIn();
            return true;
        }
        else if (dy > 0)
        {
            onZoomOut();
            return true;
        }

        return false;
    }

    bool MainWindow::onCloseRequest()
    {
        if (m_trayIcon.isVisible())
        {
            Application::getInstance().keepAlive();
            set_visible(false);
            return true;
        }
        else
        {
            Application::getInstance().endKeepAlive();
            return false;
        }
    }

    void MainWindow::onRefresh()
    {
        m_webView.refresh();
    }

    void MainWindow::onLoadStatusChanged(WebKitLoadEvent loadEvent)
    {
        if ((loadEvent == WEBKIT_LOAD_FINISHED) && !m_pendingUrl.empty())
        {
            m_webView.sendRequest(m_pendingUrl);
            m_pendingUrl.clear();
        }
    }

    void MainWindow::onOpenPreferences()
    {
        if (!m_preferencesWindow)
        {
            auto const refBuilder = Gtk::Builder::create_from_resource("/main/ui/PreferencesWindow.ui");
            m_preferencesWindow = Gtk::Builder::get_widget_derived<PreferencesWindow>(refBuilder, "window_preferences", m_trayIcon, m_webView);
        }

        m_preferencesWindow->set_transient_for(*this);
        m_preferencesWindow->set_visible(true);
    }


    void MainWindow::onNotificationChanged(bool show)
    {
        if (!is_visible())
        {
            m_trayIcon.setAttention(show);
        }
    }

    void MainWindow::onConnectivityChanged(bool connected)
    {
        if (!is_visible())
        {
            m_trayIcon.setWarning(!connected);
        }
    }

    void MainWindow::onShow()
    {
        if (!is_visible())
        {
            m_trayIcon.setAttention(false);
            m_trayIcon.setWarning(false);
        }

        set_visible(true);
        present();
    }

    void MainWindow::onQuit()
    {
        if (m_trayIcon.isVisible())
        {
            m_trayIcon.setVisible(false);
        }
        close();
    }

    void MainWindow::onFullscreen()
    {
        is_fullscreen() ? unfullscreen() : fullscreen();
    }

    void MainWindow::onZoomIn()
    {
        m_webView.zoomIn();
        m_buttonZoomLevel->set_label(m_webView.getZoomLevelString());
    }

    void MainWindow::onZoomOut()
    {
        m_webView.zoomOut();
        m_buttonZoomLevel->set_label(m_webView.getZoomLevelString());
    }

    void MainWindow::onResetZoom()
    {
        m_webView.resetZoom();
        m_buttonZoomLevel->set_label(m_webView.getZoomLevelString());
    }

    void MainWindow::onShortcuts()
    {
        if (!m_shortcutsWindow)
        {
            auto const refBuilder = Gtk::Builder::create_from_resource("/main/ui/ShortcutsWindow.ui");
            m_shortcutsWindow = refBuilder->get_widget<Gtk::ShortcutsWindow>("shortcuts_window");
        }

        m_shortcutsWindow->set_transient_for(*this);
        m_shortcutsWindow->set_visible(true);
    }

    void MainWindow::onAbout()
    {
        if (!m_aboutDialog)
        {
            m_aboutDialog = new Gtk::AboutDialog{};
            m_aboutDialog->set_version(WIL_VERSION);
            m_aboutDialog->set_program_name(_("WasIstLos"));
            m_aboutDialog->set_comments(_("An unofficial WhatsApp desktop application for Linux"));
            m_aboutDialog->set_website(WIL_HOMEPAGE);
            m_aboutDialog->set_website_label(_("GitHub Repository"));
            m_aboutDialog->set_license_type(Gtk::License::GPL_3_0);
            m_aboutDialog->set_hide_on_close(true);
        }

        if (is_visible())
        {
            m_aboutDialog->set_transient_for(*this);
        }

        m_aboutDialog->set_visible(true);
    }
}
