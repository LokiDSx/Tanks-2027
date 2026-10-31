#ifndef EAGLE_H
#define EAGLE_H

#include "object.h"


class Eagle : public Object
{
public:
    // enemy_base = true builds the Deathmatch enemy base: the same object,
    // but its wreck stays on the map (no flag switch after the explosion)
    Eagle(double x, double y, bool enemy_base = false);

    void update(Uint32 dt) override;
    void destroy();

    bool isEnemyBase() const;

private:
    bool m_enemy_base;
};

#endif // EAGLE_H
