#include "app_base.hpp"

class MyApp : public imr::AppBase {

};

int main() {

    MyApp app;
    app.run();

    return 0;
}