#pragma once

#include <gtkmm/applicationwindow.h>
#include <gtkmm/builder.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/aboutdialog.h>
#include "PreferencesWindow.hpp"
#include "TrayIcon.hpp"
#include "WebView.hpp"

namespace wil::ui
{
    class MainWindow : public Gtk::ApplicationWindow
    {
        public:
            MainWindow(BaseObjectType* cobject, Glib::RefPtr<Gtk::Builder> const& refBuilder);

        public:
            void openUrl(std::string const& url);

        protected:
            bool onKeyPressed(guint keyval, guint keycode, Gdk::ModifierType state);
            bool onScroll(double dx, double dy);
            bool onCloseRequest();

        private:
            void onRefresh();
            void onLoadStatusChanged(WebKitLoadEvent loadEvent);
            void onOpenPreferences();
            void onNotificationChanged(bool show);
            void onConnectivityChanged(bool connected);
            void onShow();
            void onQuit();
            void onFullscreen();
            void onZoomIn();
            void onZoomOut();
            void onResetZoom();
            void onShortcuts();
            void onAbout();

        private:
            TrayIcon              m_trayIcon;
            WebView               m_webView;
            Glib::ustring         m_pendingUrl;
            PreferencesWindow*    m_preferencesWindow;
            Gtk::HeaderBar*       m_headerBar;
            Gtk::ShortcutsWindow* m_shortcutsWindow;
            Gtk::AboutDialog*     m_aboutDialog;
            Gtk::Button*          m_buttonZoomLevel;
    };
}
