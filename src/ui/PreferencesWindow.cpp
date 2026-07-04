#include "PreferencesWindow.hpp"
#include <glibmm/i18n.h>
#include <gtkmm/settings.h>
#include "../util/Settings.hpp"

namespace wil::ui
{
    PreferencesWindow::PreferencesWindow(BaseObjectType* cobject, Glib::RefPtr<Gtk::Builder> const& refBuilder, TrayIcon& backgroundManager, WebView& webView)
        : Gtk::Window{cobject}
        , m_trayIcon{&backgroundManager}
        , m_webView{&webView}
        , m_switchStartInTray{nullptr}
        , m_switchStartMinimized{nullptr}
        , m_comboboxHwAccel{nullptr}
    {
        auto switchCloseToTray = refBuilder->get_widget<Gtk::Switch>("switch_close_to_tray");
        switchCloseToTray->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onCloseToTrayChanged), false);

        m_switchStartInTray = refBuilder->get_widget<Gtk::Switch>("switch_start_in_tray");
        m_switchStartInTray->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onStartInTrayChanged), false);

        m_switchStartMinimized = refBuilder->get_widget<Gtk::Switch>("switch_start_minimized");
        m_switchStartMinimized->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onStartMinimizedChanged), false);

        auto switchAutostart = refBuilder->get_widget<Gtk::Switch>("switch_autostart");
        switchAutostart->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onAutostartChanged), false);

        auto switchPreferDarkTheme = refBuilder->get_widget<Gtk::Switch>("switch_prefer_dark_theme");
        switchPreferDarkTheme->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onPreferDarkThemeChanged), false);

        m_comboboxHwAccel = refBuilder->get_widget<Gtk::ComboBoxText>("combobox_hw_accel");
        m_comboboxHwAccel->signal_changed().connect(sigc::mem_fun(*this, &PreferencesWindow::onHwAccelChanged));
        m_comboboxHwAccel->append(_("On Demand"));
        m_comboboxHwAccel->append(_("Always"));
        m_comboboxHwAccel->append(_("Never"));

        auto switchAllowPermissions = refBuilder->get_widget<Gtk::Switch>("switch_allow_permissions");
        switchAllowPermissions->signal_state_set().connect(sigc::mem_fun(*this, &PreferencesWindow::onAllowPermissionsChanged), false);

        auto spinMinFontSize = refBuilder->get_widget<Gtk::SpinButton>("spinbutton_min_font_size");
        spinMinFontSize->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &PreferencesWindow::onMinFontSizeChanged), spinMinFontSize));

        switchCloseToTray->set_state(m_trayIcon->isVisible());
        m_switchStartInTray->set_state(util::Settings::getInstance().getValue<bool>("general", "start-in-tray") && m_trayIcon->isVisible());
        m_switchStartInTray->set_sensitive(m_trayIcon->isVisible());
        m_switchStartMinimized->set_state(util::Settings::getInstance().getValue<bool>("general", "start-minimized"));
        switchAutostart->set_state(util::Settings::getInstance().getValue<bool>("general", "autostart"));
        switchPreferDarkTheme->set_state(util::Settings::getInstance().getValue<bool>("appearance", "prefer-dark-theme"));
        m_comboboxHwAccel->set_active(util::Settings::getInstance().getValue<int>("web", "hw-accel", 1));
        switchAllowPermissions->set_state(util::Settings::getInstance().getValue<bool>("web", "allow-permissions"));
        spinMinFontSize->set_value(util::Settings::getInstance().getValue<int>("web", "min-font-size", 0));
    }

    bool PreferencesWindow::onCloseToTrayChanged(bool state)
    {
        m_trayIcon->setVisible(state);
        util::Settings::getInstance().setValue("general", "close-to-tray", state);

        if (!state)
        {
            m_switchStartInTray->set_state(false);
        }
        m_switchStartInTray->set_sensitive(state);

        return false;
    }

    bool PreferencesWindow::onStartInTrayChanged(bool state) const
    {
        util::Settings::getInstance().setValue("general", "start-in-tray", state);

        if (state)
        {
            m_switchStartMinimized->set_state(false);
        }
        m_switchStartMinimized->set_sensitive(!state);

        return false;
    }

    bool PreferencesWindow::onStartMinimizedChanged(bool state) const
    {
        util::Settings::getInstance().setValue("general", "start-minimized", state);

        if (state)
        {
            m_switchStartInTray->set_state(false);
        }
        m_switchStartInTray->set_sensitive(!state);

        return false;
    }

    bool PreferencesWindow::onAutostartChanged(bool state) const
    {
        util::Settings::getInstance().setValue("general", "autostart", state);

        return false;
    }

    bool PreferencesWindow::onPreferDarkThemeChanged(bool state) const
    {
        auto const settings = Gtk::Settings::get_default();
        settings->property_gtk_application_prefer_dark_theme().set_value(state);

        util::Settings::getInstance().setValue("appearance", "prefer-dark-theme", state);

        return false;
    }

    bool PreferencesWindow::onAllowPermissionsChanged(bool state) const
    {
        util::Settings::getInstance().setValue("web", "allow-permissions", state);

        return false;
    }

    void PreferencesWindow::onHwAccelChanged() const
    {
        auto active = m_comboboxHwAccel->get_active_row_number();
        m_webView->setHwAccelPolicy(static_cast<WebKitHardwareAccelerationPolicy>(active));
        util::Settings::getInstance().setValue("web", "hw-accel", active);
    }

    void PreferencesWindow::onMinFontSizeChanged(Gtk::SpinButton* spinButtonMinFontSize) const
    {
        auto const fontSize = static_cast<int>(spinButtonMinFontSize->get_value());
        m_webView->setMinFontSize(fontSize);
        util::Settings::getInstance().setValue("web", "min-font-size", fontSize);
    }
}
