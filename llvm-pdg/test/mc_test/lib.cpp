#include <iostream>
#include <vector>
#include <string>
#include <algorithm> // 添加此行
using namespace std;

// 全局变量
vector<string> books;
int bookCount = 0;

// 添加书籍
void addBook(const string& title) {
    books.push_back(title);
    bookCount++;
}

// 借阅书籍
bool borrowBook(const string& title) {
    auto it = std::find(books.begin(), books.end(), title); // 使用 std::find
    if (it != books.end()) {
        books.erase(it);
        bookCount--;
        return true;
    }
    return false;
}

// 归还书籍
void returnBook(const string& title) {
    addBook(title);
}

// 查看所有书籍
void viewBooks() {
    cout << "当前书籍列表:\n";
    for (const auto& book : books) {
        cout << "书籍《" << book << "》\n";
    }
}

int main() {
    int choice;
    string title;

    do {
        cout << "\n图书馆管理系统\n";
        cout << "1. 添加书籍\n";
        cout << "2. 借阅书籍\n";
        cout << "3. 归还书籍\n";
        cout << "4. 查看所有书籍\n";
        cout << "5. 退出\n";
        cout << "请选择操作: ";
        cin >> choice;
        cin.ignore(); // 清除输入缓冲区

        switch (choice) {
            case 1:
                cout << "请输入书籍标题: ";
                getline(cin, title);
                addBook(title);
                break;
            case 2:
                cout << "请输入借阅书籍的标题: ";
                getline(cin, title);
                if (borrowBook(title)) {
                    cout << "成功借阅书籍《" << title << "》。\n";
                } else {
                    cout << "书籍《" << title << "》未找到。\n";
                }
                break;
            case 3:
                cout << "请输入归还书籍的标题: ";
                getline(cin, title);
                returnBook(title);
                cout << "成功归还书籍《" << title << "》。\n";
                break;
            case 4:
                viewBooks();
                break;
            case 5:
                cout << "退出程序。\n";
                break;
            default:
                cout << "无效选择，请重试。\n";
        }
    } while (choice != 5);

    return 0;
}

