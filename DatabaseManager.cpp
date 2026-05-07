#include "pch.h"
#include "DatabaseManager.h"
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "crypt32.lib")

namespace ChitalDB {

    DatabaseManager& DatabaseManager::GetInstance() {
        static DatabaseManager instance;
        return instance;
    }

    DatabaseManager::~DatabaseManager() {
        Close();
    }

    bool DatabaseManager::Initialize(const std::string& dbPath) {
        int rc = sqlite3_open(dbPath.c_str(), &db_);
        if (rc != SQLITE_OK) {
            return false;
        }

        ExecuteQuery("PRAGMA encoding = 'UTF-8';");
        ExecuteQuery("PRAGMA foreign_keys = ON;");

        if (CreateTables()) {
            AddDefaultBooks(); // Добавляем книги по умолчанию
            return true;
        }
        return false;
    }

    void DatabaseManager::Close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool DatabaseManager::CreateTables() {
        const char* createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS Users (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            FirstName TEXT NOT NULL,
            LastName TEXT NOT NULL,
            LibraryCardNumber TEXT UNIQUE NOT NULL,
            PhoneNumber TEXT UNIQUE NOT NULL,
            PasswordHash TEXT NOT NULL,
            CreatedAt DATETIME DEFAULT CURRENT_TIMESTAMP,
            LastLoginAt DATETIME,
            IsActive INTEGER DEFAULT 1
        );
    )";

        const char* createBooksTable = R"(
        CREATE TABLE IF NOT EXISTS Books (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            Title TEXT NOT NULL,
            Author TEXT,
            Genre TEXT,
            Description TEXT,
            Rating REAL DEFAULT 0,
            CoverColor TEXT,
            CreatedAt DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";

        const char* createRatingsTable = R"(
        CREATE TABLE IF NOT EXISTS Ratings (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            UserId INTEGER NOT NULL,
            BookId INTEGER NOT NULL,
            Rating REAL NOT NULL CHECK(Rating >= 0 AND Rating <= 10),
            CreatedAt DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (UserId) REFERENCES Users(Id) ON DELETE CASCADE,
            FOREIGN KEY (BookId) REFERENCES Books(Id) ON DELETE CASCADE,
            UNIQUE(UserId, BookId)
        );
    )";

        const char* createUserBooksTable = R"(
        CREATE TABLE IF NOT EXISTS UserBooks (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            UserId INTEGER NOT NULL,
            BookId INTEGER NOT NULL,
            AddedAt DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (UserId) REFERENCES Users(Id) ON DELETE CASCADE,
            FOREIGN KEY (BookId) REFERENCES Books(Id) ON DELETE CASCADE,
            UNIQUE(UserId, BookId)
        );
    )";

        return ExecuteQuery(createUsersTable) &&
            ExecuteQuery(createBooksTable) &&
            ExecuteQuery(createRatingsTable) &&
            ExecuteQuery(createUserBooksTable);
    }

    bool DatabaseManager::ExecuteQuery(const std::string& query) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            if (errMsg) {
                sqlite3_free(errMsg);
            }
            return false;
        }
        return true;
    }

    std::string DatabaseManager::HashPassword(const std::string& password) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;

        if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return "";
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return "";
        }

        if (!CryptHashData(hHash, (BYTE*)password.c_str(), (DWORD)password.length(), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        BYTE hash[32];
        DWORD hashLen = sizeof(hash);

        if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return "";
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        std::stringstream ss;
        for (DWORD i = 0; i < hashLen; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    bool DatabaseManager::RegisterUser(const std::string& firstName,
        const std::string& lastName,
        const std::string& libraryCardNumber,
        const std::string& phoneNumber,
        const std::string& password) {
        if (UserExists(phoneNumber) || LibraryCardExists(libraryCardNumber)) {
            return false;
        }

        std::string passwordHash = HashPassword(password);

        const char* query = "INSERT INTO Users (FirstName, LastName, LibraryCardNumber, PhoneNumber, PasswordHash) VALUES (?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_text(stmt, 1, firstName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, lastName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, libraryCardNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, phoneNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, passwordHash.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_DONE;
    }

    std::optional<User> DatabaseManager::LoginUser(const std::string& phoneNumber,
        const std::string& password) {
        const char* query = "SELECT Id, FirstName, LastName, LibraryCardNumber, PhoneNumber, PasswordHash FROM Users WHERE PhoneNumber = ? AND IsActive = 1;";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return std::nullopt;

        sqlite3_bind_text(stmt, 1, phoneNumber.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            std::string inputHash = HashPassword(password);

            if (storedHash == inputHash) {
                User user;
                user.Id = sqlite3_column_int(stmt, 0);
                user.FirstName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                user.LastName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                user.LibraryCardNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                user.PhoneNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

                std::string updateQuery = "UPDATE Users SET LastLoginAt = datetime('now') WHERE Id = " + std::to_string(user.Id) + ";";
                ExecuteQuery(updateQuery);

                sqlite3_finalize(stmt);
                return user;
            }
        }

        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    bool DatabaseManager::UserExists(const std::string& phoneNumber) {
        const char* query = "SELECT COUNT(*) FROM Users WHERE PhoneNumber = ?;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, phoneNumber.c_str(), -1, SQLITE_STATIC);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }

        sqlite3_finalize(stmt);
        return exists;
    }

    bool DatabaseManager::LibraryCardExists(const std::string& cardNumber) {
        const char* query = "SELECT COUNT(*) FROM Users WHERE LibraryCardNumber = ?;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, cardNumber.c_str(), -1, SQLITE_STATIC);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }

        sqlite3_finalize(stmt);
        return exists;
    }
    
    bool DatabaseManager::AddUserBook(int userId, int bookId) {
        const char* query = "INSERT OR IGNORE INTO UserBooks (UserId, BookId) VALUES (?, ?);";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return false;

        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_int(stmt, 2, bookId);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_DONE;
    }

    std::vector<Book> DatabaseManager::GetUserBooks(int userId) {
        std::vector<Book> books;

        const char* query = R"(
        SELECT b.Id, b.Title, b.Author, b.Genre, b.Description, b.Rating, b.CoverColor, ub.AddedAt
        FROM Books b
        INNER JOIN UserBooks ub ON b.Id = ub.BookId
        WHERE ub.UserId = ?
        ORDER BY ub.AddedAt DESC
    )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return books;

        sqlite3_bind_int(stmt, 1, userId);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Book book;
            book.Id = sqlite3_column_int(stmt, 0);
            book.Title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            book.Author = sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
            book.Genre = sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "";
            book.Description = sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "";
            book.Rating = sqlite3_column_double(stmt, 5);
            book.CoverColor = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "#FFD8A8";
            book.AddedAt = sqlite3_column_text(stmt, 7) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)) : "";
            books.push_back(book);
        }

        sqlite3_finalize(stmt);
        return books;
    }

    int DatabaseManager::GetUserBooksCount(int userId) {
        const char* query = "SELECT COUNT(*) FROM UserBooks WHERE UserId = ?;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, userId);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return count;
    }

    std::vector<Book> DatabaseManager::GetAllBooks() {
        std::vector<Book> books;

        const char* query = "SELECT Id, Title, Author, Genre, Description, Rating, CoverColor FROM Books ORDER BY Rating DESC;";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return books;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Book book;
            book.Id = sqlite3_column_int(stmt, 0);
            book.Title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            book.Author = sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
            book.Genre = sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "";
            book.Description = sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "";
            book.Rating = sqlite3_column_double(stmt, 5);
            book.CoverColor = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "#FFD8A8";
            books.push_back(book);
        }

        sqlite3_finalize(stmt);
        return books;
    }

    void DatabaseManager::AddDefaultBooks() {
        const char* checkQuery = "SELECT COUNT(*) FROM Books;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, checkQuery, -1, &stmt, nullptr);
        sqlite3_step(stmt);
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (count > 0) return;

        // Конвертируем строки явно через Windows API
        struct DefaultBook {
            const wchar_t* title;
            const wchar_t* author;
            const wchar_t* genre;
            const wchar_t* description;
            double rating;
            const wchar_t* color;
        };

        DefaultBook books[] = {
            {L"Дети Дюны", L"Фрэнк Герберт", L"фантастика, приключения, драма", L"Прошло 12 лет с тех пор, как Пол Муад-Диб Атрейдес, житель пустыни, задумал подчинить себе империю...", 9.7, L"#FFD8A8"},
            {L"451 градус по Фаренгейту", L"Рэй Брэдбери", L"драма, фантастика", L"В каком-то тоталитарном обществе будущего пожарные не тушат, а разжигают пожары...", 9.0, L"#FF726F"},
            {L"Мастер и Маргарита", L"Михаил Булгаков", L"роман, мистика, философия", L"Однажды весной в Москве появляется таинственный иностранец Воланд со своей свитой...", 9.8, L"#E8D5B7"},
            {L"Преступление и наказание", L"Фёдор Достоевский", L"роман, драма, психология", L"Бывший студент Родион Раскольников совершает убийство...", 9.5, L"#C4A882"},
            {L"Война и мир", L"Лев Толстой", L"роман, история, драма", L"Эпопея о русском обществе в эпоху войн против Наполеона...", 9.3, L"#B5CAA0"},
            {L"Три товарища", L"Эрих Мария Ремарк", L"роман, драма", L"История дружбы трех немецких солдат после Первой мировой войны...", 9.2, L"#A8D8EA"},
            {L"Сто лет одиночества", L"Габриэль Гарсиа Маркес", L"магический реализм, роман", L"История семьи Буэндиа в вымышленном городе Макондо...", 9.1, L"#F3D250"},
            {L"Великий Гэтсби", L"Фрэнсис Скотт Фицджеральд", L"роман, драма", L"История о богатом и загадочном Джее Гэтсби...", 8.9, L"#C9B1FF"},
            {L"1984", L"Джордж Оруэлл", L"антиутопия, фантастика", L"Тоталитарное общество под надзором Большого Брата...", 9.4, L"#FFB3BA"},
            {L"Убить пересмешника", L"Харпер Ли", L"роман, драма", L"История о расовых предрассудках в американском городке...", 9.2, L"#BAFFC9"},
            {L"Гарри Поттер и философский камень", L"Джоан Роулинг", L"фэнтези, приключения", L"Мальчик узнает, что он волшебник, и отправляется в школу магии...", 9.0, L"#FFD700"},
            {L"Тень ветра", L"Карлос Руис Сафон", L"мистика, роман", L"История о забытых книгах и тайнах Барселоны...", 9.1, L"#D4A5A5"}
        };

        const char* insertQuery = "INSERT INTO Books (Title, Author, Genre, Description, Rating, CoverColor) VALUES (?, ?, ?, ?, ?, ?);";

        for (const auto& book : books) {
            sqlite3_stmt* insertStmt;
            sqlite3_prepare_v2(db_, insertQuery, -1, &insertStmt, nullptr);

            // Конвертируем wstring в UTF-8
            auto wstringToUtf8 = [](const wchar_t* wstr) -> std::string {
                if (!wstr) return "";
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
                std::string result(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size_needed, nullptr, nullptr);
                return result;
                };

            std::string title = wstringToUtf8(book.title);
            std::string author = wstringToUtf8(book.author);
            std::string genre = wstringToUtf8(book.genre);
            std::string description = wstringToUtf8(book.description);
            std::string color = wstringToUtf8(book.color);

            sqlite3_bind_text(insertStmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertStmt, 2, author.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertStmt, 3, genre.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertStmt, 4, description.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(insertStmt, 5, book.rating);
            sqlite3_bind_text(insertStmt, 6, color.c_str(), -1, SQLITE_TRANSIENT);

            sqlite3_step(insertStmt);
            sqlite3_finalize(insertStmt);
        }
    }

    std::vector<Book> DatabaseManager::GetPopularBooks(int limit) {
        std::vector<Book> books;

        std::string query = R"(
        SELECT b.Id, b.Title, b.Author, b.Genre, b.Description, b.Rating, b.CoverColor,
               (SELECT COUNT(*) FROM UserBooks WHERE BookId = b.Id) as ReadCount
        FROM Books b
        ORDER BY b.Rating DESC
        LIMIT )" + std::to_string(limit) + ";";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return books;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Book book;
            book.Id = sqlite3_column_int(stmt, 0);
            book.Title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            book.Author = sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
            book.Genre = sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "";
            book.Description = sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "";
            book.Rating = sqlite3_column_double(stmt, 5);
            book.CoverColor = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "#FFD8A8";
            book.ReadCount = sqlite3_column_int(stmt, 7);
            books.push_back(book);
        }

        sqlite3_finalize(stmt);
        return books;
    }

    std::vector<Book> DatabaseManager::SearchBooks(const std::string& query) {
        std::vector<Book> books;

        std::string sql = R"(
        SELECT b.Id, b.Title, b.Author, b.Genre, b.Description, b.Rating, b.CoverColor,
               (SELECT COUNT(*) FROM UserBooks WHERE BookId = b.Id) as ReadCount
        FROM Books b
        WHERE b.Title LIKE ? OR b.Author LIKE ? OR b.Genre LIKE ?
        ORDER BY b.Title ASC
    )";

        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) return books;

        std::string searchPattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, searchPattern.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Book book;
            book.Id = sqlite3_column_int(stmt, 0);
            book.Title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            book.Author = sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
            book.Genre = sqlite3_column_text(stmt, 3) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) : "";
            book.Description = sqlite3_column_text(stmt, 4) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)) : "";
            book.Rating = sqlite3_column_double(stmt, 5);
            book.CoverColor = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "#FFD8A8";
            book.ReadCount = sqlite3_column_int(stmt, 7);
            books.push_back(book);
        }

        sqlite3_finalize(stmt);
        return books;
    }

    bool DatabaseManager::IsBookInUserList(int userId, int bookId) {
        const char* query = "SELECT COUNT(*) FROM UserBooks WHERE UserId = ? AND BookId = ?;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_int(stmt, 2, bookId);

        bool exists = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }

        sqlite3_finalize(stmt);
        return exists;
    }

} // namespace ChitalDB