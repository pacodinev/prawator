#pragma once

#include <cstdint>
#include <cassert>

#include "entity.hpp"

namespace WaTor {

    class Tile {
    private:
        std::uint8_t m_lastAte:4,  m_age:4;

        static constexpr unsigned LAST_ATE_BITS = 4, AGE_BITS = 4;
        static constexpr unsigned LAST_ATE_MASK = 0xFU, AGE_MASK = 0xFU;

    public:

        static constexpr unsigned MAX_AGE = 14;
        static constexpr unsigned MAX_LAST_ATE = 14;

        void set(Entity ent, unsigned age, unsigned lastAte) {
            assert(age <= MAX_AGE && lastAte <= MAX_LAST_ATE);
            switch (ent) {
                case Entity::WATER:
                    assert(age == 0 && lastAte == 0);
                    m_lastAte = 0; m_age = 0;
                    break;
                case Entity::FISH:
                    assert(lastAte == 0 && age <= 14);
                    m_lastAte = 0; m_age = (age+1) & AGE_MASK;
                    break;
                case Entity::SHARK:
                    assert(lastAte <= 14 && age <= 14);
                    m_lastAte = (lastAte+1) & LAST_ATE_MASK; 
                    m_age = (age+1) & AGE_MASK;
                    break;
            }
        }


        Tile(Entity ent, unsigned age, unsigned lastAte) { // NOLINT
            set(ent, age, lastAte);
        }

        Tile() : Tile(Entity::WATER, 0, 0) {}

        [[nodiscard]] Entity getEntity() const {
            if(m_age == 0 && m_lastAte == 0) { return Entity::WATER; }
            if(m_lastAte == 0) { return Entity::FISH; }
            return Entity::SHARK;
        }

        [[nodiscard]] unsigned getAge() const { assert(m_age>0); return m_age-1; }
        [[nodiscard]] unsigned getLastAte() const { assert(m_lastAte>0); return m_lastAte-1; }

        void setAge(unsigned age) {
            assert(m_age>0 && age <= 14); 
            m_age = (age+1) & AGE_MASK;
        }
        void setLastAte(unsigned lastAte) { 
            assert(m_lastAte>0 && lastAte <= 14); 
            m_lastAte = (lastAte+1) & LAST_ATE_MASK; 
        }

        bool operator==(const Tile& rhs) const {
            return (m_age == rhs.m_age) && (m_lastAte == rhs.m_lastAte);
        }
    };

    static_assert(sizeof(Tile) == 1, "Tile is large .. just large");
}
