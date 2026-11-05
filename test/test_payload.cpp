// Teste de host do payload IR — roda o MESMO código do firmware (game.h).
//   g++ -std=c++17 -I test/stub -I src test/test_payload.cpp -o /tmp/test_payload
#include <cstdio>
#include <cstdint>
#include "../src/game.h"

int main()
{
    int falhas = 0, casos = 0;

    // ida e volta em todas as combinações de time/jogador e vários danos
    for (int t = 0; t < 16; t++)
        for (int p = 0; p < 16; p++)
            for (int d = 1; d < 256; d += 17)
            {
                casos++;
                uint32_t c = game_make_payload(t, p, d);
                uint8_t rt = 0xFF, rp = 0xFF, rd = 0;
                if (!game_parse_payload(c, &rt, &rp, &rd))
                {
                    printf("FALHA: parse recusou t=%d p=%d d=%d (0x%08x)\n", t, p, d, c);
                    falhas++;
                    continue;
                }
                if (rt != t || rp != p || rd != d)
                {
                    printf("FALHA: t=%d/%d p=%d/%d d=%d/%d\n", t, rt, p, rp, d, rd);
                    falhas++;
                }
            }
    printf("ida e volta: %d casos, %d falhas\n", casos, falhas);

    // corrupção de 1 bit tem que ser detectada
    uint32_t base = game_make_payload(0, 3, 1);
    int pegas = 0;
    for (int bit = 0; bit < 32; bit++)
    {
        uint8_t a, b, e;
        if (!game_parse_payload(base ^ (1u << bit), &a, &b, &e))
            pegas++;
    }
    printf("corrupcao de 1 bit detectada: %d/32\n", pegas);
    if (pegas != 32) falhas++;

    // compatibilidade com os códigos antigos (firmware original)
    for (int t = 0; t < 7; t++)
    {
        uint32_t legado = 0xFFFFFFFFu - t;
        uint8_t lt, lp, ld;
        if (!game_parse_payload(legado, &lt, &lp, &ld) || lt != t || ld != DAMAGE_PER_HIT)
        {
            printf("FALHA legado 0x%08x -> time %u dano %u\n", legado, lt, ld);
            falhas++;
        }
    }
    printf("legado (0xFFFFFFFF - time): ok\n");

    // ruído tem que ser recusado
    const uint32_t lixo[] = {0x12345678, 0x00000000, 0xA5000000, 0xDEADBEEF};
    for (uint32_t l : lixo)
    {
        uint8_t a, b, e;
        if (game_parse_payload(l, &a, &b, &e))
        {
            printf("FALHA: aceitou lixo 0x%08x\n", l);
            falhas++;
        }
    }
    printf("ruido recusado: ok\n");

    printf(falhas ? "\nRESULTADO: %d FALHAS\n" : "\nRESULTADO: tudo passou (%d falhas)\n", falhas);
    return falhas ? 1 : 0;
}
