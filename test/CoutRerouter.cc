#include "test/CoutRerouter.h"

#include <iostream>

CoutRerouter::CoutRerouter() { saved = std::cout.rdbuf(sstr.rdbuf()); }

CoutRerouter::~CoutRerouter() { std::cout.rdbuf(saved); }

std::string CoutRerouter::str() const { return sstr.str(); }
