/**
 * @file game.cpp
 * @brief Estado de jogo autônomo: vida, morte, respawn, times e LEDs.
 */

#include "tagger.h"
#include "game.h"
#include <Preferences.h>

#if LIFE_PER_PLAYER != NUM_LEDS
#error "LIFE_PER_PLAYER precisa ser igual a NUM_LEDS (1 LED = 1 vida)"
#endif

static Preferences prefs;
static uint8_t s_life = LIFE_PER_PLAYER;
static uint8_t s_player = 0;

void game_begin(uint8_t *t)
{
    prefs.begin("tagger", false);
    s_life = prefs.getUChar("life", LIFE_PER_PLAYER);
    if (s_life > LIFE_PER_PLAYER) s_life = LIFE_PER_PLAYER;
    *t = prefs.getUChar("team", 0) % NUM_TEAMS;
    uint8_t mac[6] = {0};
    uint64_t chip = ESP.getEfuseMac();
    for (int i = 0; i < 6; i++) mac[i] = (chip >> (8 * i)) & 0xFF;
    s_player = mac[0] & 0x0F;
    ESP_LOGI("game", "time %u | jogador %u | vida %u", *t, s_player, s_life);
}

void game_save(uint8_t t)
{
    prefs.putUChar("life", s_life);
    prefs.putUChar("team", t);
}

void game_show_team(uint8_t t)
{
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i].setColorCode((i < s_life) ? color_team[t] : 0x000000);
    FastLED.show();
}

void game_show_life()
{
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i].setColorCode((i < s_life) ? color_team[team] : 0x000000);
    FastLED.show();
}

void game_show_downtime()
{
    for (int i = 0; i < NUM_LEDS; i++)
        leds[i].setColorCode((i < s_life) ? 0x100000 : 0x000000);   // vermelho fraco
    FastLED.show();
}

void game_flash_hit()
{
    for (int k = 0; k < HIT_FLASH_TIMES; k++)
    {
        for (int i = 0; i < NUM_LEDS; i++) leds[i].setColorCode(0xFF0000);
        FastLED.show();
        vTaskDelay(HIT_FLASH_MS / portTICK_PERIOD_MS);
        for (int i = 0; i < NUM_LEDS; i++) leds[i].setColorCode(0x000000);
        FastLED.show();
        vTaskDelay(HIT_FLASH_MS / portTICK_PERIOD_MS);
    }
}

void game_flash_friendly()
{
    for (int i = 0; i < NUM_LEDS; i++) leds[i].setColorCode(0x0000FF);
    FastLED.show();
    vTaskDelay(80 / portTICK_PERIOD_MS);
    game_show_life();
}

bool game_take_hit(uint8_t damage)
{
    if (s_life == 0)
        return false;
    s_life = (damage >= s_life) ? 0 : (s_life - damage);
    player_is_on = false;
    game_flash_hit();
    if (s_life == 0)
    {
        // morto: apaga tudo e fica fora até o respawn pelo gatilho
        for (int i = 0; i < NUM_LEDS; i++) leds[i].setColorCode(0x000000);
        FastLED.show();
        game_save(team);
        ESP_LOGI("game", "sem vida: morto ate o respawn");
        return false;
    }
    game_show_downtime();
    game_save(team);
    ESP_LOGI("game", "atingido, vida restante: %u", s_life);
    return true;
}

bool game_is_dead() { return s_life == 0; }

void game_respawn()
{
    s_life = LIFE_PER_PLAYER;
    player_is_on = true;
    game_save(team);
    game_show_life();
    ESP_LOGI("game", "respawn: vida %u", s_life);
}

uint8_t game_life() { return s_life; }

uint8_t game_player_id() { return s_player; }
