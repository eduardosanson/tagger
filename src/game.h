/**
 * @file game.h
 * @brief Regras de jogo autônomas do tagger (sem app, sem celular)
 *
 * Vida = número de LEDs (4 LEDs = 4 vidas). Cada tiro tira DAMAGE_PER_HIT.
 * Quando a vida chega a zero o jogador fica morto: não atira e a barra apaga.
 * Respawn: segurar o gatilho por RESPAWN_HOLD_MS quando estiver morto.
 * Times: NUM_TEAMS (2). Fogo amigo é ignorado pelo código do time no payload.
 *
 * Payload IR (32 bits, protocolo NEC, enviado cru pelo RMT):
 *   bits 31-24  assinatura 0xA5   (descarta controle remoto / ruído)
 *   bits 23-20  time (0-15)
 *   bits 19-16  jogador (0-15)
 *   bits 15-8   dano (1-255)
 *   bits  7-0   checksum (XOR dos três bytes acima)
 *
 * Códigos antigos (0xFFxxxxxx) continuam aceitos como dano 1, para não
 * invalidar taggers gravados com o firmware original.
 */
#ifndef game_h
#define game_h

#include <Arduino.h>

#define IR_SIGNATURE      0xA5
#define NUM_TEAMS         2
#define LIFE_PER_PLAYER   4          // vidas = nº de LEDs (verificado em game.cpp)
#define DAMAGE_PER_HIT    1
#define RESPAWN_HOLD_MS   2000
#define HIT_FLASH_MS      60
#define HIT_FLASH_TIMES   3

/* As duas funções de payload são `static inline` de propósito: assim o teste
 * de host (test/test_payload.cpp) exercita exatamente o mesmo código que roda
 * no ESP32, sem duplicar a fórmula. */
static inline uint32_t game_make_payload(uint8_t t, uint8_t p, uint8_t d)
{
    uint32_t code = ((uint32_t)IR_SIGNATURE << 24) |
                    ((uint32_t)(t & 0x0F) << 20) |
                    ((uint32_t)(p & 0x0F) << 16) |
                    ((uint32_t)d << 8);
    code |= (uint8_t)(((code >> 24) ^ (code >> 16) ^ (code >> 8)) & 0xFF);
    return code;
}

static inline bool game_parse_payload(uint32_t code, uint8_t *t, uint8_t *p, uint8_t *d)
{
    if ((code >> 24) == IR_SIGNATURE)
    {
        uint8_t check = (uint8_t)(((code >> 24) ^ (code >> 16) ^ (code >> 8)) & 0xFF);
        if (check != (uint8_t)(code & 0xFF))
            return false;                       // leitura corrompida
        if (t) *t = (code >> 20) & 0x0F;
        if (p) *p = (code >> 16) & 0x0F;
        if (d) *d = (code >> 8) & 0xFF;
        return true;
    }
    // compatibilidade com o firmware original: 0xFFFFFFFF - time
    if ((code >> 24) == 0xFF)
    {
        if (t) *t = (uint8_t)((0xFF - (code & 0xFF)) & 0x07);
        if (p) *p = 0x0F;
        if (d) *d = DAMAGE_PER_HIT;
        return true;
    }
    return false;
}

void     game_begin(uint8_t *team);      // carrega time/jogador do NVS
void     game_save(uint8_t team);        // grava time e vida no NVS
void     game_show_team(uint8_t team);
void     game_show_life();
void     game_show_downtime();
void     game_flash_hit();
void     game_flash_friendly();
bool     game_take_hit(uint8_t damage);  // true = sobreviveu, false = morreu
bool     game_is_dead();
void     game_respawn();
uint8_t  game_life();
uint8_t  game_player_id();

#endif
