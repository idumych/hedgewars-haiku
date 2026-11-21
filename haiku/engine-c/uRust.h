#pragma once

#include "pas2c.h"

#include "SDLh.h"
#include "uTypes.h"
#include "uFloat.h"
static const string255 HWEngineFutureLibName = STRINIT("hwengine_future");
typedef pointer TRGameField;
typedef pointer TRAI;
TRGameField create_empty_game_field(LongWord width,LongWord height);
void get_game_field_parameters(TRGameField game_field,LongInt (*width),LongInt (*height),LongInt (*play_width),LongInt (*play_height));
void dispose_game_field(TRGameField game_field);
Word land_get(TRGameField game_field,LongInt x,LongInt y);
void land_set(TRGameField game_field,LongInt x,LongInt y,Word value);
PWordArray land_row(TRGameField game_field,LongInt row);
void land_fill(TRGameField game_field,LongInt x,LongInt y,Word border,Word fill);
LongWord land_pixel_get(TRGameField game_field,LongInt x,LongInt y);
void land_pixel_set(TRGameField game_field,LongInt x,LongInt y,LongWord value);
PLongWordArray land_pixel_row(TRGameField game_field,LongInt row);
TRGameField generate_outline_templated_game_field(LongWord feature_size,PChar seed,PChar template_type,PChar data_path);
TRGameField generate_wfc_templated_game_field(LongWord feature_size,PChar seed,PChar template_type,PChar data_path);
TRGameField generate_maze_game_field(LongWord feature_size,PChar seed,PChar template_type,PChar data_path);
void apply_theme(TRGameField game_field,PChar data_path,PChar theme_name);
typedef LongWord TAmmoCounts[61];
typedef TAmmoCounts * PAmmoCounts;
TRAI create_ai(TRGameField game_field);
void ai_clear_team(TRAI ai);
void ai_clear_targets(TRAI ai);
void ai_add_target(TRAI ai,LongInt x,LongInt y,LongInt health,LongWord radius,single density);
void ai_add_team_hedgehog(TRAI ai,PGear gear,PAmmoCounts ammo_counts);
void ai_think(TRAI ai);
boolean ai_have_plan(TRAI ai);
void ai_get_action(TRAI ai,PGear gear,string255 (*action));
void dispose_ai(TRAI ai);
HWFloat hwf_new(LongInt numerator,LongWord denominator);
HWFloat hwf_raw(LongBool is_negative,QWord value);
double hwf_to_f64(HWFloat number);
HWFloat hwf_op_plus(HWFloat n1,HWFloat n2);
HWFloat hwf_op_minus(HWFloat n1,HWFloat n2);
HWFloat hwf_op_mul(HWFloat n1,HWFloat n2);
HWFloat hwf_op_div(HWFloat n1,HWFloat n2);
boolean hwf_op_lt(HWFloat n1,HWFloat n2);
boolean hwf_op_gt(HWFloat n1,HWFloat n2);
boolean hwf_op_eq(HWFloat n1,HWFloat n2);
boolean hwf_is_zero(HWFloat value);
boolean hwf_is_negative(HWFloat value);
HWFloat hwf_op_neg(HWFloat value);
HWFloat hwf_op_mul_int(HWFloat n1,LongInt n2);
HWFloat hwf_op_div_int(HWFloat n1,LongInt n2);
shortstring hwf_to_str(HWFloat value);
Int64 hwf_round(HWFloat value);
HWFloat hwf_abs(HWFloat value);
HWFloat hwf_sqr(HWFloat value);
HWFloat hwf_sqrt(HWFloat value);
HWFloat hwf_distance(HWFloat x,HWFloat y);
HWFloat hwf_sqr_distance(HWFloat x,HWFloat y);
HWFloat hwf_sign_as(HWFloat num,HWFloat signum);
HWFloat hwf_with_sign(HWFloat num,LongBool is_negative);
LongInt hwf_signum(HWFloat r);
HWFloat hwf_min_positive();
boolean hedgehog_step(TRGameField game_field,PGear gear);
boolean hedgehog_start_high_jump(TRGameField game_field,PGear gear);
boolean hedgehog_start_long_jump(TRGameField game_field,PGear gear);
boolean hedgehog_can_back_jump(PGear gear);
boolean hedgehog_start_back_jump(PGear gear);
void hedgehog_stop_jump(TRGameField game_field,PGear gear);
boolean hedgehog_jump_step(TRGameField game_field,PGear gear);
LongWord gear_checksum(PGear gear);
Word test_collision_x(TRGameField game_field,LongInt centerX,LongInt centerY,LongInt radius,Int8 direction,Word collisionMask);
Word test_collision_y(TRGameField game_field,LongInt centerX,LongInt centerY,LongInt radius,Int8 direction,Word collisionMask);

