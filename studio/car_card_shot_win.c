/* Windows: `car card` takes its pictures from a game run in a child process (studio/car_card_shot.c, POSIX spawn); there is no such
 * thing here yet, so only --generic works. */
#include "studio/car_card.h"

int car_card_shot(const CarCardShot *s, ExchangeError *err)
{
    (void)s;
    exchange_fail(err, "the card's pictures from the game are not available on Windows: use --generic");
    return 0;
}
