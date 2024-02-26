#include <iostream>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>

#include "Sema/Entity.h"
#include "Sema/NameMangling.h"
#include "Sema/SemaUtil.h"
#include "Sema/SimpleAnalzyer.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using test::find;
using test::lookup;
