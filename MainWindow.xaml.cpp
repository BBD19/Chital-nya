#include "pch.h" 
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Input;

namespace winrt::BookInterface::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
    }

    void MainWindow::OnMenuClicked(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
    {
        RootSplitView().IsPaneOpen(!RootSplitView().IsPaneOpen());
    }

    void MainWindow::OnMainMenuClicked(IInspectable const&, RoutedEventArgs const&)
    {
        // Закрываем боковую панель
        RootSplitView().IsPaneOpen(false);
    }

    void MainWindow::OnAccountClicked(IInspectable const&, RoutedEventArgs const&)
    {
        AuthOverlay().Visibility(Visibility::Visible);
        RootSplitView().IsPaneOpen(false); // Закрываем меню при входе
        ShowLogin(nullptr, nullptr);
    }

    void MainWindow::OnCloseAuthClicked(IInspectable const&, RoutedEventArgs const&)
    {
        AuthOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::ShowLogin(IInspectable const&, PointerRoutedEventArgs const&)
    {
        LoginFields().Visibility(Visibility::Visible);
        RegisterFields().Visibility(Visibility::Collapsed);
        LoginUnderline().Visibility(Visibility::Visible);
        RegisterUnderline().Visibility(Visibility::Collapsed);

        // Цвета - активный (синий) и неактивный (серый)
        Windows::UI::Color activeColor{ 255, 54, 84, 93 };
        Windows::UI::Color inactiveColor{ 255, 100, 116, 139 };

        LoginTabText().Foreground(SolidColorBrush(activeColor));
        RegisterTabText().Foreground(SolidColorBrush(inactiveColor));
    }

    void MainWindow::ShowRegister(IInspectable const&, PointerRoutedEventArgs const&)
    {
        LoginFields().Visibility(Visibility::Collapsed);
        RegisterFields().Visibility(Visibility::Visible);
        LoginUnderline().Visibility(Visibility::Collapsed);
        RegisterUnderline().Visibility(Visibility::Visible);

        Windows::UI::Color activeColor{ 255, 54, 84, 93 };
        Windows::UI::Color inactiveColor{ 255, 100, 116, 139 };

        RegisterTabText().Foreground(SolidColorBrush(activeColor));
        LoginTabText().Foreground(SolidColorBrush(inactiveColor));
    }

    // При нажатии на "Забыли пароль?"
    void MainWindow::OnForgotClicked(IInspectable const&, PointerRoutedEventArgs const&)
    {
        // Прячем заголовки входа и поля
        AuthHeader().Visibility(Visibility::Collapsed);
        LoginFields().Visibility(Visibility::Collapsed);
        RegisterFields().Visibility(Visibility::Collapsed);

        // Показываем заголовок и текст "Забыли пароль"
        ForgotHeader().Visibility(Visibility::Visible);
        ForgotFields().Visibility(Visibility::Visible);
    }

    // При нажатии на крестик в окне "Забыли пароль?" (возврат назад)
    void MainWindow::OnBackToLoginClicked(IInspectable const&, RoutedEventArgs const&)
    {
        // Прячем блок забытого пароля
        ForgotHeader().Visibility(Visibility::Collapsed);
        ForgotFields().Visibility(Visibility::Collapsed);

        // Возвращаем обычный заголовок и вкладку входа
        AuthHeader().Visibility(Visibility::Visible);
        ShowLogin(nullptr, nullptr);
    }
}