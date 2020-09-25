// Generates code to read & write data from json files and strings
// Code meant for use with rapidjson
// RapidJSON Website: https://rapidjson.org/
// RapidJSON Github:  https://github.com/Tencent/rapidjson/
#pragma once

#include "df_serialize/test/rapidjson/document.h"
#include "df_serialize/test/rapidjson/error/en.h"

// Reading
#include "../df_serialize/df_serialize/MakeJSONReadHeader.h"
#include "schemas.h"
#include "../df_serialize/df_serialize/MakeJSONReadFooter.h"