#include "historical_deployment.h"
#include <iostream>
#include <stdexcept>

void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
const Formation& find(const FormationWorld& world,unsigned id) {
    for(const auto& entry:world.formations)if(entry.id==id)return entry.formation;
    throw std::runtime_error("Missing test formation");
}
WorldFormation single(unsigned id,unsigned side,float x,float z,float heading=0) {
    WorldFormation entry;entry.id=id;entry.side=side;auto& f=entry.formation;f.x=x;f.z=z;f.heading=heading;f.strength=20;
    for(unsigned g=0;g<25;++g){f.organization.smallGroups[g].strength=g==12?20.0f:0;f.organization.smallGroups[g].heading=heading;}
    return entry;
}
void meleeTests() {
    FormationWorld world;world.reset({single(10,4,-2.6f,-8),single(90,7,2.6f,-8,3.14159265f),
        single(30,4,-2.6f,8),single(70,7,2.6f,8,3.14159265f)});
    world.update(10);require(find(world,10).strength==20 && world.impacts.empty(),"Paused combat caused damage");
    world.running=true;auto reordered=world,split=world;std::reverse(reordered.formations.begin(),reordered.formations.end());
    world.update(8);reordered.update(8);for(unsigned tick=0;tick<480;++tick)split.update(1.0f/60);
    for(const auto& entry:world.formations) {
        const auto& f=entry.formation;
        require(f.strength<20 && f.strength>0 && f.morale<100,"Contact did not cause actual losses and morale reduction");
        require(f.strength==find(reordered,entry.id).strength && f.strength==find(split,entry.id).strength,"Combat depends on storage order or timestep");
        require(f.organization.smallGroups[12].activeFighters<=f.organization.smallGroups[12].faceDeployment.total()+.001f,"Undeployed troops attacked");
        require(entry.targets[12] && entry.targets[12]->group==12 && entry.targets[12]->formation!=entry.id,"Target lost formation/group identity");
    }
    require(std::abs(find(world,10).strength-find(world,90).strength)<.001f,"Symmetric battle gives first attacker an advantage");
    require(!world.impacts.empty() && world.impacts.front().damage>0,"Actual combat produced no impacts");
    world.hold(10);const float before=find(world,10).strength;world.update(1);
    require(find(world,10).strength<before && find(world,10).x==-2.6f,"Hold stopped combat or allowed movement");
    auto allies=world;for(auto& entry:allies.formations)entry.side=4;
    const float friendlyBefore=find(allies,10).strength;allies.update(1);
    require(find(allies,10).strength==friendlyBefore && allies.impacts.empty(),"Friendly fire or stale impacts");
    FormationWorld blocked;blocked.reset({single(2,0,0,0),single(9,1,7,0,3.14159265f),single(3,0,3.5f,0)});
    blocked.formations[2].formation.organization.smallGroups[12].resting=true;
    blocked.running=true;blocked.update(4);
    require(find(blocked,2).strength==20 && find(blocked,9).strength==20,"Melee passed through a resting friendly blocker");
    auto resting=single(3,0,0,0);resting.formation.organization.smallGroups[12].resting=true;
    FormationWorld restTarget;restTarget.reset({resting,single(9,1,5.2f,0,3.14159265f)});restTarget.running=true;restTarget.update(4);
    require(find(restTarget,3).strength<20 && find(restTarget,9).strength==20,"Resting troops became invulnerable or attacked");
    FormationWorld distant;distant.reset({single(1,0,0,0),single(2,1,20,0)});distant.running=true;distant.update(3);
    require(find(distant,1).strength==20 && distant.impacts.empty(),"Out-of-range damage");
    FormationWorld surrounded;surrounded.reset({single(100,5,0,0),single(1,9,5.2f,0),single(2,9,-5.2f,0),single(3,9,0,5.2f),single(4,9,0,-5.2f)});
    surrounded.running=true;
    for(unsigned tick=0;tick<600;++tick) {
        surrounded.update(1.0f/60);const auto& g=find(surrounded,100).organization.smallGroups[12];
        require(g.activeFighters<=g.faceDeployment.accountedStrength+.001f && g.faceDeployment.total()<=g.faceDeployment.accountedStrength+.001f,
            "Surrounded group allocated its troops more than once");
    }
    require(find(surrounded,1).strength<20 && find(surrounded,2).strength<20 && find(surrounded,3).strength<20 && find(surrounded,4).strength<20,
        "Contact allocation failed to reach multiple enemy formations");
    auto dead=single(40,4,0,0);dead.formation.organization.smallGroups[12].strength=0;dead.formation.strength=0;
    FormationWorld exhausted;exhausted.reset({dead,single(80,7,5.2f,0)});exhausted.running=true;exhausted.move(40,{30,0});exhausted.update(1);
    exhausted.hold(40);
    require(find(exhausted,40).defeated() && find(exhausted,40).x==0 && find(exhausted,80).strength==20,"Empty army moved, attacked or revived");
    auto tinyA=single(1,0,-2.6f,0),tinyB=single(2,1,2.6f,0,3.14159265f);
    for(auto* entry:{&tinyA,&tinyB}){entry->formation.strength=.0001f;entry->formation.organization.smallGroups[12].strength=.0001f;}
    FormationWorld mutual;mutual.reset({tinyA,tinyB});mutual.running=true;mutual.update(1.0f/60);
    require(find(mutual,1).strength==0 && find(mutual,2).strength==0 && mutual.impacts.size()==2,"Simultaneous lethal damage was not applied to both sides");
    world.reset({single(5,0,0,0)});require(world.impacts.empty() && !world.formations[0].targets[12] && world.time==0,"Reset retained combat state");
}
void automaticTests() {
    FormationWorld world;world.reset({single(10,0,-20,-15),single(90,1,20,-15,3.14159265f),
        single(30,0,-20,15),single(70,1,20,15,3.14159265f)});
    world.advanceAll();world.update(3);require(world.time==0 && find(world,10).x==-20,"Automatic order bypassed pause");
    world.running=true;auto reordered=world,split=world;std::reverse(reordered.formations.begin(),reordered.formations.end());
    world.update(16);reordered.update(16);for(unsigned tick=0;tick<960;++tick)split.update(1.0f/60);
    for(const auto& entry:world.formations) {
        const auto& f=entry.formation;
        require(f.strength<20,"Automatic army did not reach combat");
        require(f.x==find(reordered,entry.id).x && f.z==find(reordered,entry.id).z && f.strength==find(reordered,entry.id).strength &&
            f.x==find(split,entry.id).x && f.strength==find(split,entry.id).strength,"Automatic decisions depend on order or timestep");
    }
    world.hold(10);const auto held=find(world,10);world.update(1);
    require(find(world,10).x==held.x && world.formations[0].order==WorldOrder::Hold,"Auto advance overrode hold");
    world.move(10,{-40,-15});world.update(1);
    require(find(world,10).x<held.x && find(world,10).targetX==-40 && world.formations[0].order==WorldOrder::Move,"Auto advance overrode manual move");
    FormationWorld targets;targets.reset({single(12,0,0,0),single(99,1,30,0),single(55,1,0,50)});targets.running=true;targets.advance(12);targets.update(1.0f/60);
    require(targets.formations[0].advanceTarget==99,"Automatic target is not nearest enemy ID");
    targets.formations[1].formation.strength=0;targets.formations[1].formation.organization.smallGroups[12].strength=0;targets.update(1.0f/60);
    require(targets.formations[0].advanceTarget==55,"Destroyed enemy was not replaced");
    targets.formations[2].side=0;targets.update(1.0f/60);
    require(!targets.formations[0].advanceTarget && !find(targets,12).moving,"Army kept moving after losing all enemies");
    targets.reset({single(12,0,0,0)});require(targets.formations[0].order==WorldOrder::Hold && !targets.formations[0].advanceTarget,"Reset retained autonomous orders");

    WorldFormation red,blue;red.id=10;blue.id=80;blue.side=1;red.formation.x=-13;blue.formation.x=13;
    FormationWorld reserves;reserves.reset({red,blue});
    for(unsigned g=4;g<25;g+=5)reserves.formations[0].formation.organization.smallGroups[g].strength=0;
    reserves.formations[0].formation.strength=400;reserves.advance(10);reserves.running=true;reserves.update(4);
    require(reserves.formations[0].order==WorldOrder::Advance && find(reserves,10).x>-13 && find(reserves,80).strength<500,
        "Loss of front rank caused premature withdrawal or blocked surviving ranks");

    const auto wear=[](FormationWorld& battle) {
        for(auto& g:battle.formations[0].formation.organization.smallGroups)g.strength=6;
        battle.formations[0].formation.strength=150;battle.advance(10);battle.running=true;
    };
    red.formation.x=0;blue.formation.x=40;
    FormationWorld tired;tired.reset({red,blue});wear(tired);tired.update(1);
    require(tired.formations[0].order==WorldOrder::Advance,"Withdrawal ignored persistence delay");
    tired.update(4);require(tired.formations[0].order==WorldOrder::Withdraw && find(tired,10).x<0,"Exhausted army failed to withdraw");
    const auto retreatTarget=find(tired,10).targetX;tired.advanceAll();tired.update(25);
    require(tired.formations[0].order==WorldOrder::Withdraw && find(tired,10).targetX==retreatTarget && std::abs(find(tired,10).x-retreatTarget)<.001f,
        "Withdrawing army recharged or failed to stop at destination");
    WorldFormation rear=red;rear.id=22;rear.formation.x=-26;blue.formation.x=26;
    FormationWorld trapped;trapped.reset({red,blue,rear});wear(trapped);trapped.update(5);
    require(trapped.formations[0].order==WorldOrder::Withdraw && find(trapped,10).movementBlocked,"Blocked retreat did not stop");
    for(unsigned a=0;a<25;++a)for(unsigned b=0;b<25;++b)
        require(battleDistance(find(trapped,10).groupPosition(a),find(trapped,22).groupPosition(b))>=4.5f,"Retreat passed through friendly army");
    red.formation.x=-436;blue.formation.x=-406;
    FormationWorld edge;edge.reset({red,blue});wear(edge);edge.update(30);
    require(edge.formations[0].order==WorldOrder::Withdraw && find(edge,10).x>=-436 && find(edge,10).targetX>=-436,"Retreat escaped battlefield");

    auto deployment=makeDeployment(history::sekigahara());deployment.advanceAll();deployment.running=true;
    unsigned firstContact=0;
    for(unsigned second=1;second<=60;++second) {
        deployment.update(1);
        for(const auto& entry:deployment.formations)if(entry.formation.strength<500 && firstContact==0)firstContact=second;
    }
    unsigned damaged=0;for(const auto& entry:deployment.formations)damaged+=entry.formation.strength<500;
    require(firstContact>0 && damaged>=2,"Sekigahara initial deployment did not reach autonomous combat");
    std::cout<<"Sekigahara automatic advance: first contact by "<<firstContact<<"s, "<<damaged<<" armies damaged at 60s\n";
}
int main() {
    try {
        auto scenario=history::sekigahara();auto deployment=makeDeployment(scenario);
        require(deployment.formations.size()==14 && !deployment.running,"Invalid initial deployment");
        const auto poses=deploymentPoses(deployment);
        for(unsigned id=0;id<14;++id) {
            const auto original=history::sample(scenario.armies[id],scenario.startMinute);
            require(std::abs(poses[id].x-original.x)<.001f && std::abs(poses[id].z-original.z)<.001f && poses[id].side==original.side,
                "Deployment lost initial position or side");
        }
        std::vector<WorldFormation> entries;
        for(unsigned id=0;id<4;++id) {
            WorldFormation entry;entry.id=10+id*7;entry.side=id%2;
            entry.formation.x=id%2?30.0f:-30.0f;entry.formation.z=id<2?0:60.0f;entries.push_back(entry);
        }
        FormationWorld world;world.combatEnabled=false;world.reset(entries);
        require(world.nearestEnemy(10)==17 && world.nearestEnemy(24)==31,"Enemy selection confused ID and side");
        auto sameSide=entries;for(auto& entry:sameSide)entry.side=8;
        FormationWorld allies;allies.reset(sameSide);require(!allies.nearestEnemy(10),"Friendly formation selected as enemy");
        auto duplicate=entries;duplicate[1].id=duplicate[0].id;bool rejected=false;
        try{world.reset(duplicate);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && world.formations.size()==4,"Duplicate ID was accepted or damaged current world");
        for(const auto& entry:entries)world.move(entry.id,{-entry.formation.x,entry.formation.z});
        world.update(2);require(world.time==0 && find(world,10).x==-30,"Paused world moved");
        world.running=true;
        auto split=world,reordered=world;std::reverse(reordered.formations.begin(),reordered.formations.end());
        world.update(20);reordered.update(20);for(unsigned tick=0;tick<1200;++tick)split.update(1.0f/60);
        for(const auto& entry:world.formations) {
            const auto& f=entry.formation;const auto& s=find(split,entry.id);const auto& r=find(reordered,entry.id);
            require(f.x==s.x && f.z==s.z && f.x==r.x && f.z==r.z,"Movement depends on cadence or storage order");
            require(f.movementBlocked && f.strength==500,"Colliding armies failed to stop or lost troops");
            for(const auto& other:world.formations)if(entry.id!=other.id)for(unsigned a=0;a<25;++a)for(unsigned b=0;b<25;++b)
                require(battleDistance(f.groupPosition(a),other.formation.groupPosition(b))>=4.5f,"World formations overlap");
        }
        // 同じ陣営でも同じ占有検査を使う。
        allies.running=true;for(const auto& entry:entries)allies.move(entry.id,{-entry.formation.x,entry.formation.z});allies.update(20);
        require(find(allies,10).x==find(world,10).x,"Friendly collision differs from enemy collision");
        world.move(10,{-60,0});world.update(1);const float held=find(world,10).x;
        world.hold(10);world.update(1);require(find(world,10).x==held,"Hold did not stop a single formation");
        world.reset(entries);require(world.time==0 && !world.running && find(world,10).x==-30,"Reset retained commands or positions");
        world.combatEnabled=true;world.running=true;
        for(const auto& entry:entries)world.move(entry.id,{-entry.formation.x,entry.formation.z});
        world.update(20);
        for(const auto& entry:world.formations)require(entry.formation.strength<500 && entry.formation.movementBlocked,"Marching armies failed to fight at contact");
        meleeTests();
        automaticTests();
        std::cout<<"Multiple-formation movement, contact allocation, simultaneous damage, impacts and Sekigahara deployment passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
