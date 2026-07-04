#include "Application.hpp"
#include <gtkmm/window.h>
#include <gtk/gtk.h>
#include "Config.hpp"
#include "../util/Settings.hpp"

namespace wil::ui
{
    Application* Application::m_instance = nullptr;

    Application& Application::getInstance()
    {
        return *m_instance;
    }

    Application::Application()
        : Gtk::Application{WIL_APP_ID, Gio::Application::Flags::HANDLES_OPEN}
        , m_onHold{false}
        , m_mainWindow{nullptr}
    {
        if (!m_instance)
        {
            m_instance = this;
        }
        else
        {
            throw std::runtime_error("Attempt to create more than one instance of the Application");
        }
    }

    void Application::keepAlive()
    {
        if (!m_onHold)
        {
            hold();
            m_onHold = true;
        }
    }

    void Application::endKeepAlive()
    {
        if (m_onHold)
        {
            release();
            m_onHold = false;
        }
    }

    void Application::on_startup()
    {
        Gtk::Application::on_startup();

        if (auto* display = gdk_display_get_default())
        {
            auto* iconTheme = gtk_icon_theme_get_for_display(display);
            gtk_icon_theme_add_resource_path(iconTheme, "/main/image/icons/hicolor");
        }

        auto const refBuilder = Gtk::Builder::create_from_resource("/main/ui/MainWindow.ui");

        m_mainWindow.reset(Gtk::Builder::get_widget_derived<wil::ui::MainWindow>(refBuilder, "window_main"));

        add_window(*m_mainWindow);
    }

    void Application::on_activate()
    {
        Gtk::Application::on_activate();

        if (wil::util::Settings::getInstance().getValue<bool>("general", "start-in-tray")
            && wil::util::Settings::getInstance().getValue<bool>("general", "close-to-tray"))
        {
            if (!m_onHold)
            {
                keepAlive();
                m_mainWindow->hide();
            }
            else
            {
                m_mainWindow->present();
            }
        }
        else if (wil::util::Settings::getInstance().getValue<bool>("general", "start-minimized"))
        {
            m_mainWindow->is_visible() ? m_mainWindow->minimize() : m_mainWindow->present();
        }
        else
        {
            m_mainWindow->present();
        }

        auto const preferDark = util::Settings::getInstance().getValue<bool>("appearance", "prefer-dark-theme");
        g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", preferDark, nullptr);
    }

    void Application::on_open(Application::type_vec_files const& files, Glib::ustring const&)
    {
        if (!files.empty())
        {
            activate();

            if (m_mainWindow)
            {
                m_mainWindow->openUrl(files.at(0U)->get_uri());
            }
        }
    }
}
