#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <sqlite3.h>

namespace ChitalDB {

    struct User {
        int Id;
        std::string FirstName;
        std::string LastName;
        std::string LibraryCardNumber;
        std::string PhoneNumber;
        std::string PasswordHash;
        std::string CreatedAt;
        std::string LastLoginAt;
        bool IsActive;
    };

    struct Book {
        int Id;
        std::string Title;
        std::string Author;
        std::string Genre;
        std::string Description;
        double Rating;
        std::string CoverColor;
        std::string AddedAt;
        int ReadCount;  // Сколько раз прочитали
    };

    class DatabaseManager {
    public:
        static DatabaseManager& GetInstance();

        bool Initialize(const std::string& dbPath);
        void Close();

        // Операции с пользователями
        bool RegisterUser(const std::string& firstName,
            const std::string& lastName,
            const std::string& libraryCardNumber,
            const std::string& phoneNumber,
            const std::string& password);

        std::optional<User> LoginUser(const std::string& phoneNumber,
            const std::string& password);

        bool UserExists(const std::string& phoneNumber);
        bool LibraryCardExists(const std::string& cardNumber);

        // Операции с книгами пользователя
        bool AddUserBook(int userId, int bookId);
        std::vector<Book> GetUserBooks(int userId);
        int GetUserBooksCount(int userId);

        // Операции с каталогом книг
        std::vector<Book> GetAllBooks();
        std::vector<Book> GetPopularBooks(int limit = 10);
        std::vector<Book> SearchBooks(const std::string& query);
        bool AddBookToCatalog(const Book& book);
        bool IsBookInUserList(int userId, int bookId);

    private:
        DatabaseManager() = default;
        ~DatabaseManager();
        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;

        bool CreateTables();
        bool ExecuteQuery(const std::string& query);
        std::string HashPassword(const std::string& password);
        void AddDefaultBooks();

        sqlite3* db_ = nullptr;
    };

} // namespace ChitalDB