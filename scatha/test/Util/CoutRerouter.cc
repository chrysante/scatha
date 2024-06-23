#include "Util/CoutRerouter.h"

#include <iostream>

using namespace scatha;
using namespace test;

CoutRerouter::CoutRerouter() { saved = std::cout.rdbuf(sstr.rdbuf()); }

CoutRerouter::~CoutRerouter() { std::cout.rdbuf(saved); }

void CoutRerouter::reset() { sstr.str({}); }

std::string CoutRerouter::str() const { return sstr.str(); }
