#ifndef CUSTOMER_UI_H
#define CUSTOMER_UI_H

#ifdef __cplusplus
class Display;
extern "C" {
#endif
/**
 * @brief Create customer custom UI
 * 
 * This function creates the customer's custom user interface.
 * It should be called after the display is initialized.
 * 
 * @param display Pointer to the Display instance created by the board
 */
#ifdef __cplusplus
void create_customer_ui(Display *display);
#else
void create_customer_ui(void *display);
#endif

#ifdef __cplusplus
}
#endif

#endif // CUSTOMER_UI_H

