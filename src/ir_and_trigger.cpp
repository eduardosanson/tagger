/**
 * @file ir_and_trigger.cpp
 * @author gldhnchn (gldhnchn@posteo.de)
 * @brief infrared stuff and trigger handler
 * @date 2019-01-23
 *
 */

#include "ir_and_trigger.h"

/**
 * @brief handle incoming infrared
 * This function reads the incoming infrared data and sends it via bluetooth.
 * @param parameter just a dummy parameter
 */
void handle_ir(void *parameter)
{
    ESP_LOGD(logtag, "handle IR task started");
    ESP_LOGD(logtag, "init ir recv front");
    ir_recv_front.setPreferred(IR_PROTOCOL);
    ir_recv_front.start(IR_RECV_FRONT_PIN);
    while (true)
    {
        if (ir_recv_front.available())
        {
            ESP_LOGD(logtag, "IR available");
            char *rcvGroup;
            uint32_t result = ir_recv_front.read(rcvGroup);
            if (result)
            {
                ESP_LOGD(logtag, "Received: %s/0x%x", rcvGroup, result);
                if (device_connected)
                {
                    ir_receive_char->setValue(result);
                    ble_notify(ir_receive_char);
                }
                else
                {
                    uint8_t t_rx = 0, p_rx = 0, d_rx = 0;
                    if (!game_parse_payload(result, &t_rx, &p_rx, &d_rx))
                    {
                        ESP_LOGD(logtag, "codigo desconhecido: 0x%x", result);
                    }
                    else if ((t_rx == team) && !TEST_SELF_HIT)
                    {
                        ESP_LOGI(logtag, "fogo amigo (time %u), ignorado", t_rx);
                        game_flash_friendly();
                    }
                    else if (game_is_dead())
                    {
                        ESP_LOGI(logtag, "tiro recebido enquanto morto, ignorado");
                    }
                    else
                    {
                        ESP_LOGI(logtag, "ACERTO do time %u, jogador %u, dano %u", t_rx, p_rx, d_rx);
                        if (game_take_hit(d_rx))
                            vTaskResume(xHandle_handle_player_status);   // downtime de 3 s
                        else
                            vTaskResume(xHandle_handle_player_status);   // morreu: task dorme
                    }
                }
            }
        }
    }
    return;
}

void handle_trigger()
{
    last_bounce_time = xTaskGetTickCount();
    portENTER_CRITICAL_ISR(&mux);
    count_trigger_interrupts++;
    portEXIT_CRITICAL_ISR(&mux);
    vTaskResume(xHandle_refresh_trigger_status);
    return;
}

void refresh_trigger_status(void *parameter)
{
    const char *logtag = "trigger";

    ESP_LOGD(logtag, "refresh trigger status task started");
    while (true)
    {
        vTaskSuspend(NULL); //suspend task until reactivated by handle_trigger()
        //wait until the last bounce is longer ago than DEBOUNCETIME
        while (xTaskGetTickCount() - last_bounce_time < DEBOUNCETIME)
            vTaskDelay(10 / portTICK_PERIOD_MS);
        //refresh trigger.pressed
        trigger.read_pin();
        ESP_LOGI(logtag, "Button Interrupt Triggered times: %u", count_trigger_interrupts);
        ESP_LOGI(logtag, "time in ms since last trigger: %u", xTaskGetTickCount() - last_bounce_time);
        ESP_LOGI(logtag, "trigger status: %u", trigger.pressed);
        last_time_button_pressed_timestamp = millis();
        //Three different tagger situations are handled here. The place for doing that feels a bit weird to me, so it maybe should be refacored.
        //1: Tagger is connected via BT
        if (device_connected)
        {
            ESP_LOGD(logtag, "sending trigger status via bt");
            trigger_char->setValue((int &)trigger.pressed);
            ble_notify(trigger_char);
        }
        //2: Team selection mode
        else if (team_selection && trigger.pressed)
        {
            team = (team + 1) % NUM_TEAMS;
            ESP_LOGI(logtag, "trocando time. Time: %u", team);
            game_show_team(team);
        }
        //3: morto -> segurar o gatilho para voltar
        else if (game_is_dead())
        {
            if (trigger.pressed)
            {
                uint32_t t0 = millis();
                while (trigger.pressed && (millis() - t0) < RESPAWN_HOLD_MS)
                {
                    vTaskDelay(20 / portTICK_PERIOD_MS);
                    trigger.read_pin();
                }
                if (!trigger.pressed && (millis() - t0) >= RESPAWN_HOLD_MS)
                {
                    ESP_LOGI(logtag, "respawn por gatilho segurado (%lu ms)", millis() - t0);
                    game_respawn();
                    vTaskResume(xHandle_handle_player_status);
                }
            }
        }
        //4: BT-less playing mode
        else if (trigger.pressed && player_is_on)
        {
            uint32_t payload = game_make_payload(team, game_player_id(), DAMAGE_PER_HIT);
            ESP_LOGI(logtag, "disparo IR: 0x%08x (time %u, jogador %u)", payload, team, game_player_id());
            ir_led.send(payload);
            // flash curto de disparo na barra, sem apagar a barra de vida
            for (int i = 0; i < NUM_LEDS; i++) leds[i].setColorCode(0xFFFFFF);
            FastLED.show();
            vTaskDelay(40 / portTICK_PERIOD_MS);
            game_show_life();
        }
        portENTER_CRITICAL_ISR(&mux);
        count_trigger_interrupts = 0;
        portEXIT_CRITICAL_ISR(&mux);
    }
}

void handle_player_status(void *parameter)
{
    ESP_LOGD(logtag, "handle player status task started");
    while (true)
    {
        if (game_is_dead())
        {
            ESP_LOGI(logtag, "morto: aguardando respawn pelo gatilho");
            player_is_on = false;
            vTaskSuspend(NULL);   // só acorda em game_respawn()
            continue;             // ao acordar, vida cheia -> volta a ativo
        }
        ESP_LOGI(logtag, "jogador ativo (vida %u)", game_life());
        player_is_on = true;
        game_show_life();
        vTaskSuspend(NULL);   // acorda em handle_ir() quando leva um tiro
        ESP_LOGI(logtag, "jogador em downtime");
        player_is_on = false;
        game_show_downtime();
        vTaskDelay(PLAYER_DOWNTIME_IN_MS / portTICK_PERIOD_MS);
    }
    return;
}