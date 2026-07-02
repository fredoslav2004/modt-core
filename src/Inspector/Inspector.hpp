/*
 * MODT (Modeling Tool)
 * Copyright (C) 2026 Eduard Fekete <modt@eduardfekete.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef INSPECTOR_HPP
#define INSPECTOR_HPP

#include "../Model/Model.hpp"
#include <filesystem>
#include <vector>

namespace Inspector {

int run(const Model::Project& project, const std::vector<std::filesystem::path>& sourceFiles);

} // namespace Inspector

#endif // INSPECTOR_HPP
