#include "pet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 _Static_assert(sizeof(pet_v1_t)==12,"legacy save ABI");
 _Static_assert(sizeof(pet_t)==28,"v2 save ABI");
 pet_t p;pet_new(&p);assert(pet_valid(&p)&&pet_stage(&p)==0);
 assert(!pet_act(&p,PET_FEED));pet_advance(&p,59);assert(pet_stage(&p)==0);
 pet_advance(&p,1);assert(pet_stage(&p)==1&&p.fullness==78&&p.energy==98);
 pet_advance(&p,1740);assert(pet_stage(&p)==3&&p.age==1800&&p.form==PET_MELLOW);
 p.energy=30;assert(pet_act(&p,PET_SLEEP));assert(!pet_act(&p,PET_PLAY));
 pet_advance(&p,600);assert(p.energy==100&&p.sleeping);
 assert(pet_act(&p,PET_SLEEP));assert(!p.sleeping);
 p.energy=7;assert(!pet_act(&p,PET_PLAY));p.energy=100;p.fullness=100;p.mood=99;
 assert(pet_act(&p,PET_PLAY));assert(p.energy==92&&p.fullness==97&&p.mood==100);
 assert(pet_act(&p,PET_FEED));assert(p.fullness==100);p.clean=0;
 assert(pet_act(&p,PET_CLEAN)&&p.clean==100);assert(!pet_act(&p,99));
 pet_advance(&p,0xFFFFFFFFu);assert(pet_valid(&p)&&p.age==31536000u);
 pet_advance(&p,0xFFFFFFFFu);assert(pet_valid(&p));assert(p.fullness==0&&p.energy==0&&p.clean==0);
 p.version=3;assert(!pet_valid(&p));pet_new(&p);p.reserved[0]=1;assert(!pet_valid(&p));
 pet_new(&p);p.sleeping=2;assert(!pet_valid(&p));
 pet_new(&p);pet_t other=p;
 for(unsigned i=0;i<3600;i++)pet_advance(&p,1);
 for(unsigned i=0;i<60;i++)pet_advance(&other,60);
 assert(!memcmp(&p,&other,sizeof(p)));
 pet_new(&p);for(unsigned i=0;i<30;i++){p.fullness=p.mood=p.energy=p.clean=100;pet_advance(&p,60);}
 assert(p.form==PET_SUNNY&&pet_valid(&p));
 pet_new(&p);pet_advance(&p,60);p.coins=9995;
 for(unsigned i=0;i<5;i++){p.energy=p.fullness=100;assert(pet_game(&p,true));}
 assert(p.coins==9999&&p.games_won==5);pet_advance(&p,1740);assert(p.form==PET_SPORT);
 unsigned mask=p.dex;pet_new_egg(&p);assert(p.age==0&&!p.form&&p.dex==mask&&p.coins==9999);
 assert(pet_buy(&p,PET_PLANT));unsigned coins=p.coins;
 assert(!pet_buy(&p,PET_PLANT)&&p.coins==coins);assert(pet_use(&p,PET_PLANT)&&p.decor==1);
 assert(pet_use(&p,PET_ITEMS)&&!p.decor);assert(!pet_use(&p,PET_BALL));
 p.items[PET_APPLE]=9;assert(!pet_buy(&p,PET_APPLE)&&p.coins==coins);
 pet_advance(&p,60);p.fullness=50;assert(pet_use(&p,PET_APPLE)&&p.fullness==85&&p.items[0]==8);
 p.sleeping=1;assert(!pet_use(&p,PET_APPLE));p.sleeping=0;p.coins=0;
 assert(!pet_buy(&p,PET_SOAP));assert(!pet_buy(&p,99));assert(!pet_use(&p,99));
 pet_v1_t legacy={.age=900,.version=1,.fullness=77,.mood=66,.energy=88,.clean=99,.sleeping=1};
 assert(pet_migrate(&p,&legacy)&&pet_valid(&p));assert(p.age==900&&p.sleeping&&pet_stage(&p)==2&&p.coins==30);
 legacy.version=2;assert(!pet_migrate(&p,&legacy));
 puts("PASS pet v2: migration, four stages/three forms, economy/limits, inventory/decor, dex retained, old care regression");
}
