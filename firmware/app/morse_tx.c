/* Legacy explicit-send entry point delegates to the sole transport owner. */
#include "morse_tx.h"
#include "morse_ui.h"
void morse_tx_init(void) {}
bool morse_tx_send_plain(const char *text, uint16_t len)
{ return morse_ui_send_plain_text(text, len); }