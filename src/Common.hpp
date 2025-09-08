#pragma once

#include <vector>
#include <string>
#include <cctype>
#include <map>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stack>
#include <memory>
#include <functional>
#include <random>
#include <any>
#include <algorithm>
#include <utility>
#include <thread>
#include "MagicEnum.hpp"

#define exfunc ExternalFunction
#define def = [](const std::vector<std::any> &Args) -> std::any
#define ret return nullptr;
#define forarg for (const std::any &Arg : Args)