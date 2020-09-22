#include "df_serialize/test/rapidjson/document.h"
#include "df_serialize/test/rapidjson//error/en.h"

#include "schemas/types.h"

#define MAKE_JSON_LOG(...) printf(__VA_ARGS__);
#include "schemas/json.h"

int main(int argc, char** argv)
{
    const char* fileName = "./assets/clip.json";

    Data::Root root;
    if (!ReadFromJSON(root, fileName))
        return 1;

    return 0;
}

/*

TODO:
* might need polymorphic types. already thinking about having an array of whatever types of things

*/