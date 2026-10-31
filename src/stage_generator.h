#ifndef STAGE_GENERATOR_H
#define STAGE_GENERATOR_H

#include <string>
#include <vector>

namespace StageGenerator
{
    /**
     * Generates a procedurally built, horizontally symmetric stage layout.
     * Returns exactly AppConfig::map_size.h lines of AppConfig::map_size.w characters.
     * Symbols match the stage file format: '.' empty, '#' brick, '@' stone,
     * '%' bush, '~' water, '-' ice. The eagle area is left empty.
     * With with_eagle = false (survival mode) no eagle nest is reserved.
     * With with_water_stone = false (survival mode) water and stone blocks
     * are never generated.
     */
    std::vector<std::string> generate(int stage_number, bool with_eagle = true, bool with_water_stone = true);
}

#endif // STAGE_GENERATOR_H
