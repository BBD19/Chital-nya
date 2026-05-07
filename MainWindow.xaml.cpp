#include "pch.h" 
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "DatabaseManager.h"
#include <winrt/Windows.Storage.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::Chital::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Инициализация базы данных
        try {
            auto localFolder = Windows::Storage::ApplicationData::Current().LocalFolder();
            std::string dbPath = winrt::to_string(localFolder.Path()) + "\\chitalnya.db";

            if (!ChitalDB::DatabaseManager::GetInstance().Initialize(dbPath)) {
                OutputDebugString(L"Failed to initialize database\n");
            }
            else {
                OutputDebugString(L"Database initialized successfully\n");
            }
        }
        catch (...) {
            OutputDebugString(L"Exception during DB init\n");
        }
    }

    void MainWindow::OnMenuClicked(IInspectable const& /*sender*/, RoutedEventArgs const& /*args*/)
    {
        RootSplitView().IsPaneOpen(!RootSplitView().IsPaneOpen());
    }

    void MainWindow::OnMainMenuClicked(IInspectable const&, RoutedEventArgs const&)
    {
        RootSplitView().IsPaneOpen(false);
    }

    void MainWindow::OnAccountClicked(IInspectable const&, RoutedEventArgs const&)
    {
        RootSplitView().IsPaneOpen(false);

        if (currentUser_.has_value()) {
            LoadUserProfile();
            ProfileOverlay().Visibility(Visibility::Visible);
        }
        else {
            AuthOverlay().Visibility(Visibility::Visible);
            ShowLogin(nullptr, nullptr);
        }
    }

    void MainWindow::OnCloseAuthClicked(IInspectable const&, RoutedEventArgs const&)
    {
        AuthOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::OnCloseProfileClicked(IInspectable const&, RoutedEventArgs const&)
    {
        ProfileOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::ShowLogin(IInspectable const&, PointerRoutedEventArgs const&)
    {
        LoginFields().Visibility(Visibility::Visible);
        RegisterFields().Visibility(Visibility::Collapsed);
        LoginUnderline().Visibility(Visibility::Visible);
        RegisterUnderline().Visibility(Visibility::Collapsed);

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

    void MainWindow::OnForgotClicked(IInspectable const&, PointerRoutedEventArgs const&)
    {
        AuthHeader().Visibility(Visibility::Collapsed);
        LoginFields().Visibility(Visibility::Collapsed);
        RegisterFields().Visibility(Visibility::Collapsed);
        ForgotHeader().Visibility(Visibility::Visible);
        ForgotFields().Visibility(Visibility::Visible);
    }

    void MainWindow::OnBackToLoginClicked(IInspectable const&, RoutedEventArgs const&)
    {
        ForgotHeader().Visibility(Visibility::Collapsed);
        ForgotFields().Visibility(Visibility::Collapsed);
        AuthHeader().Visibility(Visibility::Visible);
        ShowLogin(nullptr, nullptr);
    }

    void MainWindow::OnRegisterClicked(IInspectable const&, RoutedEventArgs const&)
    {
        auto firstName = FirstNameBox().Text();
        auto lastName = LastNameBox().Text();
        auto cardNumber = LibraryCardBox().Text();
        auto phone = PhoneBox().Text();
        auto password = PasswordRegBox().Password();

        if (firstName.empty() || lastName.empty() || cardNumber.empty() ||
            phone.empty() || password.empty()) {
            return;
        }

        auto& db = ChitalDB::DatabaseManager::GetInstance();

        if (db.RegisterUser(
            winrt::to_string(firstName),
            winrt::to_string(lastName),
            winrt::to_string(cardNumber),
            winrt::to_string(phone),
            winrt::to_string(password))) {

            auto user = db.LoginUser(winrt::to_string(phone), winrt::to_string(password));
            if (user.has_value()) {
                currentUser_ = user;
                AuthOverlay().Visibility(Visibility::Collapsed);
                LoadUserProfile();
                ProfileOverlay().Visibility(Visibility::Visible);
            }
        }
    }

    void MainWindow::OnLoginClicked(IInspectable const&, RoutedEventArgs const&)
    {
        auto phone = LoginPhoneBox().Text();
        auto password = LoginPasswordBox().Password();

        if (phone.empty() || password.empty()) {
            return;
        }

        auto& db = ChitalDB::DatabaseManager::GetInstance();
        auto user = db.LoginUser(winrt::to_string(phone), winrt::to_string(password));

        if (user.has_value()) {
            currentUser_ = user;
            AuthOverlay().Visibility(Visibility::Collapsed);
            LoadUserProfile();
            ProfileOverlay().Visibility(Visibility::Visible);
        }
    }

    void MainWindow::OnLogoutClicked(IInspectable const&, RoutedEventArgs const&)
    {
        currentUser_ = std::nullopt;
        ProfileOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::LoadUserProfile() {
        if (!currentUser_.has_value()) return;

        auto& user = currentUser_.value();

        ProfileNameText().Text(winrt::to_hstring(user.FirstName + " " + user.LastName));
        ProfileCardText().Text(winrt::to_hstring("Билет №" + user.LibraryCardNumber));
        ProfilePhoneText().Text(winrt::to_hstring(user.PhoneNumber));

        std::string initials = user.FirstName.substr(0, 1);
        ProfileInitialsText().Text(winrt::to_hstring(initials));

        auto& db = ChitalDB::DatabaseManager::GetInstance();
        auto userBooks = db.GetUserBooks(user.Id);
        int booksCount = db.GetUserBooksCount(user.Id);

        ProfileBooksCountText().Text(winrt::to_hstring(std::to_string(booksCount) + " книг"));

        UserBooksList().Children().Clear();

        if (userBooks.empty()) {
            Controls::TextBlock noBooksText;
            noBooksText.Text(L"У вас пока нет прочитанных книг");
            noBooksText.FontSize(16);
            noBooksText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 100, 116, 139 }));
            noBooksText.HorizontalAlignment(HorizontalAlignment::Center);
            noBooksText.Margin(ThicknessHelper::FromUniformLength(20));
            UserBooksList().Children().Append(noBooksText);
            return;
        }

        // Сохраняем значения в переменные
        Windows::UI::Text::FontWeight boldWeight{ 700 };
        Windows::UI::Text::FontWeight semiBoldWeight{ 600 };

        for (const auto& book : userBooks) {
            Controls::Border bookCard;
            bookCard.Background(SolidColorBrush(Windows::UI::Color{ 255, 255, 255, 255 }));
            bookCard.CornerRadius(CornerRadiusHelper::FromUniformRadius(12));
            bookCard.Padding(ThicknessHelper::FromUniformLength(15));
            bookCard.Margin(ThicknessHelper::FromLengths(0, 0, 0, 10));
            bookCard.BorderBrush(SolidColorBrush(Windows::UI::Color{ 255, 226, 232, 240 }));
            bookCard.BorderThickness(ThicknessHelper::FromUniformLength(1));

            Controls::StackPanel cardContent;
            cardContent.Spacing(8);

            Controls::TextBlock titleText;
            titleText.Text(winrt::to_hstring(book.Title));
            titleText.FontSize(18);
            titleText.FontWeight(boldWeight);
            titleText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 30, 41, 59 }));

            Controls::TextBlock authorText;
            authorText.Text(winrt::to_hstring(book.Author));
            authorText.FontSize(14);
            authorText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 100, 116, 139 }));

            Controls::TextBlock genreText;
            genreText.Text(winrt::to_hstring(book.Genre));
            genreText.FontSize(13);
            genreText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 45, 106, 97 }));
            genreText.FontWeight(semiBoldWeight);

            Controls::TextBlock dateText;
            dateText.Text(winrt::to_hstring("Добавлено: " + book.AddedAt));
            dateText.FontSize(11);
            dateText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 148, 163, 184 }));
            dateText.HorizontalAlignment(HorizontalAlignment::Right);

            cardContent.Children().Append(titleText);
            cardContent.Children().Append(authorText);
            cardContent.Children().Append(genreText);
            cardContent.Children().Append(dateText);

            bookCard.Child(cardContent);
            UserBooksList().Children().Append(bookCard);
        }
    }

    void MainWindow::OnCatalogClicked(IInspectable const&, RoutedEventArgs const&)
    {
        RootSplitView().IsPaneOpen(false);
        LoadPopularBooks();
        LoadCatalog();
        CatalogOverlay().Visibility(Visibility::Visible);
    }

    void MainWindow::OnCloseCatalogClicked(IInspectable const&, RoutedEventArgs const&)
    {
        CatalogOverlay().Visibility(Visibility::Collapsed);
    }

    void MainWindow::OnCatalogSearchClicked(IInspectable const&, RoutedEventArgs const&)
    {
        auto searchText = CatalogSearchBox().Text();
        if (searchText.empty()) {
            LoadPopularBooks();
            LoadCatalog();
        }
        else {
            LoadPopularBooks();
            LoadCatalog(winrt::to_string(searchText));
        }
    }

    void MainWindow::LoadPopularBooks() {
        PopularBooksGrid().Items().Clear();

        auto& db = ChitalDB::DatabaseManager::GetInstance();
        auto popularBooks = db.GetPopularBooks(6);

        for (const auto& book : popularBooks) {
            // Создаем карточку книги (компактную)
            Controls::Border bookCard;
            bookCard.Width(280);
            bookCard.Height(200);
            bookCard.Background(SolidColorBrush(Windows::UI::Color{ 255, 255, 255, 255 }));
            bookCard.CornerRadius(CornerRadiusHelper::FromUniformRadius(15));
            bookCard.Padding(ThicknessHelper::FromUniformLength(15));
            bookCard.Margin(ThicknessHelper::FromLengths(0, 0, 15, 15));
            bookCard.BorderBrush(SolidColorBrush(Windows::UI::Color{ 255, 232, 236, 239 }));
            bookCard.BorderThickness(ThicknessHelper::FromUniformLength(1));

            Controls::Grid cardGrid;
            cardGrid.ColumnSpacing(15);

            Controls::ColumnDefinition col1;
            col1.Width(GridLengthHelper::FromPixels(80));
            Controls::ColumnDefinition col2;
            col2.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

            cardGrid.ColumnDefinitions().Append(col1);
            cardGrid.ColumnDefinitions().Append(col2);

            // Обложка книги (цветной прямоугольник)
            Controls::Border coverRect;
            coverRect.Width(80);
            coverRect.Height(110);
            coverRect.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));

            // Парсим цвет
            std::string colorStr = book.CoverColor;
            if (colorStr.size() == 7 && colorStr[0] == '#') {
                int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                coverRect.Background(SolidColorBrush(Windows::UI::Color{ 255, (uint8_t)r, (uint8_t)g, (uint8_t)b }));
            }

            cardGrid.SetColumn(coverRect, 0);

            // Информация о книге
            Controls::StackPanel infoPanel;
            infoPanel.Spacing(5);

            Controls::TextBlock titleText;
            titleText.Text(winrt::to_hstring(book.Title));
            titleText.FontSize(16);
            titleText.FontWeight(Windows::UI::Text::FontWeight{ 700 });
            titleText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 30, 41, 59 }));
            titleText.TextWrapping(TextWrapping::Wrap);
            titleText.MaxLines(2);

            Controls::TextBlock authorText;
            authorText.Text(winrt::to_hstring(book.Author));
            authorText.FontSize(13);
            authorText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 100, 116, 139 }));

            // Рейтинг
            Controls::StackPanel ratingPanel;
            ratingPanel.Orientation(Orientation::Horizontal);
            ratingPanel.Spacing(5);

            Controls::FontIcon starIcon;
            starIcon.Glyph(L"\xE735");
            starIcon.FontSize(14);
            starIcon.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 255, 179, 71 }));

            Controls::TextBlock ratingText;
            ratingText.Text(winrt::to_hstring(std::to_string(book.Rating).substr(0, 3)));
            ratingText.FontSize(13);
            ratingText.FontWeight(Windows::UI::Text::FontWeight{ 600 });
            ratingText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 255, 179, 71 }));

            ratingPanel.Children().Append(starIcon);
            ratingPanel.Children().Append(ratingText);

            // Кнопка "Читать"
            Controls::Button readButton;
            readButton.Content(winrt::box_value(L"Прочитал"));
            readButton.FontSize(12);
            readButton.FontWeight(Windows::UI::Text::FontWeight{ 600 });
            readButton.Background(SolidColorBrush(Windows::UI::Color{ 255, 54, 84, 93 }));
            readButton.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 255, 255, 255 }));
            readButton.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
            readButton.Height(30);
            readButton.Margin(ThicknessHelper::FromLengths(0, 5, 0, 0));

            int bookId = book.Id;
            readButton.Click([this, bookId](IInspectable const&, RoutedEventArgs const&) {
                if (currentUser_.has_value()) {
                    auto& db = ChitalDB::DatabaseManager::GetInstance();
                    if (db.AddUserBook(currentUser_->Id, bookId)) {
                        // Обновить UI
                    }
                }
                });

            infoPanel.Children().Append(titleText);
            infoPanel.Children().Append(authorText);
            infoPanel.Children().Append(ratingPanel);
            infoPanel.Children().Append(readButton);

            cardGrid.SetColumn(infoPanel, 1);

            cardGrid.Children().Append(coverRect);
            cardGrid.Children().Append(infoPanel);

            bookCard.Child(cardGrid);
            PopularBooksGrid().Items().Append(bookCard);
        }
    }

    void MainWindow::LoadCatalog(const std::string& searchQuery) {
        AllBooksGrid().Items().Clear();

        auto& db = ChitalDB::DatabaseManager::GetInstance();
        std::vector<ChitalDB::Book> catalogBooks;

        if (searchQuery.empty()) {
            catalogBooks = db.GetAllBooks();
        }
        else {
            catalogBooks = db.SearchBooks(searchQuery);
        }

        if (catalogBooks.empty()) {
            Controls::TextBlock noBooksText;
            noBooksText.Text(L"Книги не найдены");
            noBooksText.FontSize(16);
            noBooksText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 100, 116, 139 }));
            noBooksText.HorizontalAlignment(HorizontalAlignment::Center);
            noBooksText.Margin(ThicknessHelper::FromUniformLength(20));
            AllBooksGrid().Items().Append(noBooksText);
            return;
        }

        for (const auto& book : catalogBooks) {
            Controls::Border bookCard;
            bookCard.Width(250);
            bookCard.Background(SolidColorBrush(Windows::UI::Color{ 255, 255, 255, 255 }));
            bookCard.CornerRadius(CornerRadiusHelper::FromUniformRadius(15));
            bookCard.Padding(ThicknessHelper::FromUniformLength(12));
            bookCard.Margin(ThicknessHelper::FromLengths(0, 0, 15, 15));
            bookCard.BorderBrush(SolidColorBrush(Windows::UI::Color{ 255, 232, 236, 239 }));
            bookCard.BorderThickness(ThicknessHelper::FromUniformLength(1));

            Controls::StackPanel cardContent;
            cardContent.Spacing(8);

            // Цветная полоса сверху
            Controls::Border colorStrip;
            colorStrip.Height(100);
            colorStrip.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));

            std::string colorStr = book.CoverColor;
            if (colorStr.size() == 7 && colorStr[0] == '#') {
                int r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                int g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                int b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                colorStrip.Background(SolidColorBrush(Windows::UI::Color{ 255, (uint8_t)r, (uint8_t)g, (uint8_t)b }));
            }

            Controls::TextBlock titleText;
            titleText.Text(winrt::to_hstring(book.Title));
            titleText.FontSize(16);
            titleText.FontWeight(Windows::UI::Text::FontWeight{ 700 });
            titleText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 30, 41, 59 }));
            titleText.MaxLines(2);
            titleText.TextWrapping(TextWrapping::Wrap);

            Controls::TextBlock authorText;
            authorText.Text(winrt::to_hstring(book.Author));
            authorText.FontSize(13);
            authorText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 100, 116, 139 }));

            Controls::TextBlock genreText;
            genreText.Text(winrt::to_hstring(book.Genre));
            genreText.FontSize(12);
            genreText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 45, 106, 97 }));
            genreText.FontWeight(Windows::UI::Text::FontWeight{ 600 });

            // Рейтинг и счетчик прочтений
            Controls::Grid statsGrid;
            Controls::ColumnDefinition statCol1;
            statCol1.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            Controls::ColumnDefinition statCol2;
            statCol2.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            statsGrid.ColumnDefinitions().Append(statCol1);
            statsGrid.ColumnDefinitions().Append(statCol2);

            Controls::StackPanel ratingPanel;
            ratingPanel.Orientation(Orientation::Horizontal);
            ratingPanel.Spacing(3);

            Controls::FontIcon starIcon;
            starIcon.Glyph(L"\xE735");
            starIcon.FontSize(12);
            starIcon.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 255, 179, 71 }));

            Controls::TextBlock ratingText;
            ratingText.Text(winrt::to_hstring(std::to_string(book.Rating).substr(0, 3)));
            ratingText.FontSize(12);
            ratingText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 255, 179, 71 }));

            ratingPanel.Children().Append(starIcon);
            ratingPanel.Children().Append(ratingText);

            Controls::TextBlock readCountText;
            readCountText.Text(winrt::to_hstring("Прочли " + std::to_string(book.ReadCount) + " раз"));
            readCountText.FontSize(11);
            readCountText.Foreground(SolidColorBrush(Windows::UI::Color{ 255, 148, 163, 184 }));
            readCountText.HorizontalAlignment(HorizontalAlignment::Right);

            statsGrid.SetColumn(ratingPanel, 0);
            statsGrid.SetColumn(readCountText, 1);

            statsGrid.Children().Append(ratingPanel);
            statsGrid.Children().Append(readCountText);

            cardContent.Children().Append(colorStrip);
            cardContent.Children().Append(titleText);
            cardContent.Children().Append(authorText);
            cardContent.Children().Append(genreText);
            cardContent.Children().Append(statsGrid);

            bookCard.Child(cardContent);
            AllBooksGrid().Items().Append(bookCard);
        }
    }
}