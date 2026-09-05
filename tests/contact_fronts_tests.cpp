#include "simulation.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        std::array<std::array<ContactBody, 25>, 2> bodies{};
        bodies[0][0] = {{0, 0}, 0, true, true};
        bodies[1][0] = {{6, 0}, 3.14159265f, true, true};
        const auto frontal = measureContactFronts(bodies, 0, 0);
        require(frontal.faces[0].width() == 4.5f && frontal.faces[1].width() == 0 && frontal.faces[2].width() == 0 &&
            frontal.faces[3].width() == 0, "Frontal contact duplicated another face");
        require(measureContactFronts(bodies, 1, 0).faces[0].width() == 4.5f, "Swapping armies changed frontal width");
        for (unsigned orientation = 0; orientation < 16; ++orientation) {
            auto rotated = bodies;
            const float angle = orientation * 3.14159265f / 8;
            for (auto& team : rotated) for (auto& body : team) if (body.present) {
                const auto p = body.position;
                body.position = {12 + p.x * std::cos(angle) - p.z * std::sin(angle),
                    -9 + p.x * std::sin(angle) + p.z * std::cos(angle)};
                body.heading += angle;
            }
            require(measureContactFronts(rotated, 0, 0).faces[0].width() == 4.5f, "Contact depended on world axes or origin");
        }
        auto shifted = bodies;
        shifted[1][0].position.z = 5.5f;
        require(measureContactFronts(shifted, 0, 0).faces[0].width() < 4.5f, "Lateral offset did not reduce frontage");
        auto distant = bodies; distant[1][0].position.x = 7.51f;
        require(measureContactFronts(distant, 0, 0).faces[0].width() == 0, "Contact exceeded face reach");
        auto screened = bodies;
        screened[0][1] = {{3, 0}, 0, true, false};
        require(measureContactFronts(screened, 0, 0).faces[0].width() == 0, "Contact passed through a blocking runner");
        auto surrounded = bodies;
        surrounded[1][1] = {{-6, 0}, 0, true, true};
        const auto twoSides = measureContactFronts(surrounded, 0, 0);
        require(twoSides.faces[0].enemyWidths[0] == 4.5f && twoSides.faces[2].enemyWidths[1] == 4.5f,
            "Simultaneous front and rear contacts lost their opponents");
        const auto close = [](float a, float b) { return std::abs(a - b) < 0.0001f; };
        const auto oneFace = allocateContactFronts(frontal, 20);
        require(close(oneFace.faces[0].fighters(), 5) && close(oneFace.reserve, 15),
            "Narrow frontage committed the entire group");
        const auto moreReserves = allocateContactFronts(frontal, 40);
        require(close(moreReserves.fighters(), oneFace.fighters()) && close(moreReserves.reserve, 35),
            "Adding depth bypassed frontage capacity");
        const auto bothFaces = allocateContactFronts(twoSides, 20);
        require(close(bothFaces.faces[0].fighters(), 5) && close(bothFaces.faces[2].fighters(), 5) && close(bothFaces.reserve, 10),
            "Second front failed to use reserves");
        const auto depleted = allocateContactFronts(twoSides, 6);
        require(close(depleted.faces[0].fighters(), 3) && close(depleted.faces[2].fighters(), 3) && close(depleted.reserve, 0),
            "Front and rear duplicated scarce soldiers");
        for (float strength : {0.0f, 0.25f, 3.7f, 6.0f, 20.0f}) {
            const auto allocation = allocateContactFronts(twoSides, strength);
            require(close(allocation.fighters() + allocation.reserve, strength), "Allocation lost or created soldiers");
            for (unsigned face = 0; face < 4; ++face)
                require(allocation.faces[face].fighters() <= twoSides.faces[face].width() / ContactAllocation::widthPerFighter + 0.0001f,
                    "Face allocation exceeded its width capacity");
        }
        const auto noContact = allocateContactFronts({}, 17.5f);
        require(noContact.fighters() == 0 && noContact.reserve == 17.5f, "Unengaged group lost its reserves");
        auto shared = bodies;
        shared[1][0].position.z = 2.3f;
        shared[1][1] = {{6, -2.3f}, 3.14159265f, true, true};
        const auto split = measureContactFronts(shared, 0, 0).faces[0];
        require(split.enemyWidths[0] > 0 && split.enemyWidths[1] > 0 && split.width() <= 4.5f,
            "Multiple enemies duplicated or monopolized a shared frontage");
        const auto sharedAllocation = allocateContactFronts(measureContactFronts(shared, 0, 0), 2.5f);
        require(sharedAllocation.faces[0].enemyFighters[0] > 0 && sharedAllocation.faces[0].enemyFighters[1] > 0 &&
            sharedAllocation.fighters() <= 2.5001f && close(sharedAllocation.fighters() + sharedAllocation.reserve, 2.5f),
            "Two enemies reused the same committed soldiers");
        auto swappedFronts = twoSides;
        std::swap(swappedFronts.faces[0], swappedFronts.faces[2]);
        const auto swappedAllocation = allocateContactFronts(swappedFronts, 3.7f);
        const auto originalAllocation = allocateContactFronts(twoSides, 3.7f);
        require(close(swappedAllocation.faces[0].fighters(), originalAllocation.faces[2].fighters()),
            "Face ordering changed allocation priority");
        auto routed = bodies; routed[1][0].fighting = false;
        require(measureContactFronts(routed, 0, 0).faces[0].width() == 0, "Routed target became an active combat front");
        BattleSimulation simulation;
        simulation.toggle(); simulation.update(10);
        const auto before = simulation;
        const auto observation = simulation.contactFronts(0, 22);
        (void)observation;
        require(simulation.time == before.time && simulation.formations[0].strength == before.formations[0].strength &&
            simulation.formations[1].strength == before.formations[1].strength, "Observation changed battle state");
        simulation.result = BattleResult::RedVictory;
        for (const auto& face : simulation.contactFronts(0, 22).faces) require(face.width() == 0, "Finished battle kept contact fronts");
        std::cout << "Contact front geometry checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
