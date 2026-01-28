#include <iostream>
#include <string>

using namespace std;

class Person {
    public:
        Person(std::string n, int h) {
            this->name = n;
            this->age = h;
        }

        void changeName(std::string new_name) {
            this->name = new_name;
        }

        int getAge() {
            if (age > 100)
                throw std::string("age is too large!");
            return age;
        }

        void printInfo() {
            cout << "name: " << this->name << "\n";
            if (age < 10)
                throw std::string("Age is less than 0!");
            cout << "age: " << this->age << "\n";
        }

    private:
        std::string name;
        int age;
};

int main(int argc, char *argv[])
{
    Person *p = new Person("J", -1);
    p->changeName("K");
    try {
        p->printInfo();
        int age = p->getAge();
        cout << std::to_string(age);
    } catch (std::string msg) {
        cout << msg << "\n";
    } catch (int msg) {
        cout << "error type: " << msg << "\n";
    }

    return 0;
}
