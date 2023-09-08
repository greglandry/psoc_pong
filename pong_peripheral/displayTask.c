/******************************************************************************
* File Name: displayTask.c
*
* Description: This task updates the TFT display
*
******************************************************************************/

// MCU Headers
#include "cy_pdl.h"
#include "cyhal.h"

// BT Stack
#include "wiced_bt_stack.h"

// BT app utils
#include "app_bt_utils.h"

// BT configurator generated headers
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"

// Display headers
#include "GUI.h"

// srand(), rand()
#include "stdlib.h"

// Display task header file
#include "displayTask.h"

/*******************************************************************************
* Global Variables
********************************************************************************/
/* Task handle for this task. */
TaskHandle_t display_task_handle;

ball gameBall = {.dim = 10, .posX = 260, .posY = 120, .prev_posX = 260, .prev_posY = 120, .speedX = -1, .speedY = 0};

paddle gamePaddle = {.dimX = 10, .dimY = 40, .posX = 0, .posY = 100, .prev_posX = 0, .prev_posY = 100};

// int to count the number of bounces
uint32_t numBounces = 0;

// bool to record where the ball is
bool hasBall = true;

// Connection id declared in main.c
extern uint16_t connection_id;

/*******************************************************************************
* Function Name: resetGamePositions
********************************************************************************
* Summary:
*  Resets the game objects to their default positions
*
* Return:
*  void
*
*******************************************************************************/
void resetGamePositions() {
	gameBall.posX = 260;
	gameBall.posY = 120;
	gameBall.speedY = 0;
	gameBall.speedX = -1;
	gamePaddle.posY = 100;
	numBounces = 0;

	// Erase old paddle
	GUI_ClearRect(gamePaddle.prev_posX, gamePaddle.prev_posY, gamePaddle.prev_posX + gamePaddle.dimX, gamePaddle.prev_posY + gamePaddle.dimY);
	// Draw new paddle
	GUI_FillRect(gamePaddle.posX, gamePaddle.posY, gamePaddle.posX + gamePaddle.dimX, gamePaddle.posY + gamePaddle.dimY);
	// Update previous position
	gamePaddle.prev_posY = 100;
}

/*******************************************************************************
* Function Name: displayTask
********************************************************************************
* Summary:
*  Updates the display with current positions of the game objects
*
* Return:
*  void
*
*******************************************************************************/
void displayTask(void *arg) {
	(void)arg;

	// Generate true random numbers for ball's vertical speed upon bouncing off the paddle
	cyhal_trng_t trng_obj;
	uint32_t rdmNum;

	// Initialize the true random number generator block
	cyhal_trng_init(&trng_obj);
	// Initialize rand() with a true random number
	rdmNum = cyhal_trng_generate(&trng_obj);
	// Free true random number generator
	cyhal_trng_free(&trng_obj);
	// Seed rand with true random number
	srand(rdmNum);

	// Clear the display, reset paddle position, and draw the paddle
	GUI_Clear();
	gamePaddle.posY = 100;
	GUI_FillRect(gamePaddle.posX, gamePaddle.posY, gamePaddle.posX + gamePaddle.dimX, gamePaddle.posY + gamePaddle.dimY);

	for(;;) {
		// Draw the ball and calculate its next position if it is on the screen
		if(hasBall) {
			GUI_FillRect(gameBall.posX, gameBall.posY, gameBall.posX + gameBall.dim, gameBall.posY + gameBall.dim);
			// Update the ball's previous position
			gameBall.prev_posX = gameBall.posX;
			gameBall.prev_posY = gameBall.posY;

			// Limit refresh rate
			vTaskDelay(10);

			// Send BT notification when ball goes off far wall
			if(gameBall.posX >= SCREEN_MAX_X && gameBall.speedX > 0) {
				if(connection_id) /* Check if we have an active connection */
				{
					/* Check to see if the client has asked for notifications */
					if(app_pong_ball_client_char_config[0] & GATT_CLIENT_CONFIG_NOTIFICATION) {
						// Update characterisic values with the ball values
						app_pong_ball[0] = gameBall.posX;
						app_pong_ball[1] = gameBall.posX >> 8;
						app_pong_ball[2] = (SCREEN_MAX_Y - gameBall.posY);
						app_pong_ball[3] = (SCREEN_MAX_Y - gameBall.posY) >> 8;
						app_pong_ball[4] = (-1 * gameBall.speedX);
						app_pong_ball[5] = (-1 * gameBall.speedX) >> 8;
						app_pong_ball[6] = (gameBall.speedY * -1);
						app_pong_ball[7] = (gameBall.speedY * -1) >> 8;
						// Send notification
						wiced_bt_gatt_server_send_notification(connection_id, HDLC_PONG_BALL_VALUE, app_pong_ball_len, app_pong_ball, NULL);
						printf("Sending Game Ball Data to Central via Notification:\nposX: %d\nposY: %d\nspeedX: %d\nspeedY: %d\n", gameBall.posX,
							   gameBall.posY, gameBall.speedX, gameBall.speedY);
						hasBall = false;
					} else {
						printf("Notifications not enabled! Stopping execution!\n");
						printf("app_pong_ball_client_char_config: 0x%x, 0x%x\n", app_pong_ball_client_char_config[0],
							   app_pong_ball_client_char_config[1]);
						CY_ASSERT(0);
					}
				} else {
					printf("Connection error! Stopping execution!\n");
					CY_ASSERT(0);
				}
			}

			// If the ball goes past the paddle, reset it
			if(gameBall.posX + gameBall.dim < 0) {
				vTaskDelay(1000);
				// Don't allow interrupts while resetting paddle
				taskENTER_CRITICAL();
				resetGamePositions();
				taskEXIT_CRITICAL();
			}

			// Calculate the ball's new position
			gameBall.posX += gameBall.speedX;
			gameBall.posY += gameBall.speedY;

			// Bounce off sides of screen
			if(gameBall.posY <= 0 || gameBall.posY >= SCREEN_MAX_Y - gameBall.dim) {
				gameBall.speedY *= -1;
			}

			// Don't allow interrupts while bounces are calculated
			taskENTER_CRITICAL();

			// If the ball is intersecting with an area around the paddle that is one pixel larger than the paddle, and it is moving in the negative X direction, bounce it
			if(!(gameBall.posX > gamePaddle.posX + gamePaddle.dimX + 1) && !(gameBall.posY + gameBall.dim + 1 < gamePaddle.posY) &&
			   !(gamePaddle.posY + gamePaddle.dimY + 1 < gameBall.posY) && (gameBall.speedX < 0)) {
				// When bouncing off the front of the paddle, vertical speed is a random number between -2 and 2
				if(gameBall.posX >= gamePaddle.posX + gamePaddle.dimX) {
					gameBall.speedY = rand() % 4 - 2;
				}
				// When bouncing off the top of the paddle, vertical speed is a random number between -2 and -1
				else if(gameBall.posY <= gamePaddle.posY) {
					gameBall.speedY = rand() % 1 - 2;
				}
				// When bouncing off the bottom of the paddle, vertical speed is a random number between 1 and 2
				else if(gameBall.posY + gameBall.dim >= gamePaddle.posY + gamePaddle.dimY) {
					gameBall.speedY = rand() % 1 + 1;
				}
				numBounces++;
				gameBall.speedX = numBounces;
			}

			// Erase the old ball
			GUI_ClearRect(gameBall.prev_posX, gameBall.prev_posY, gameBall.prev_posX + gameBall.dim, gameBall.prev_posY + gameBall.dim);

			// If the previously drawn ball is intersecting with an area around the paddle that is one pixel larger than the paddle, redraw the paddle. This keeps the ball from erasing parts of the paddle
			if(!(gameBall.prev_posX > gamePaddle.posX + gamePaddle.dimX + 1) && !(gameBall.prev_posY + gameBall.dim + 1 < gamePaddle.posY) &&
			   !(gamePaddle.posY + gamePaddle.dimY + 1 < gameBall.prev_posY)) {
				GUI_ClearRect(gamePaddle.prev_posX, gamePaddle.prev_posY, gamePaddle.prev_posX + gamePaddle.dimX,
							  gamePaddle.prev_posY + gamePaddle.dimY);
				GUI_FillRect(gamePaddle.posX, gamePaddle.posY, gamePaddle.posX + gamePaddle.dimX, gamePaddle.posY + gamePaddle.dimY);
				gamePaddle.prev_posY = gamePaddle.posY;
			}
			taskEXIT_CRITICAL();
		}

		// Erase old paddle and draw new one if it has moved
		if(gamePaddle.prev_posY != gamePaddle.posY) {
			taskENTER_CRITICAL();
			GUI_ClearRect(gamePaddle.prev_posX, gamePaddle.prev_posY, gamePaddle.prev_posX + gamePaddle.dimX, gamePaddle.prev_posY + gamePaddle.dimY);
			GUI_FillRect(gamePaddle.posX, gamePaddle.posY, gamePaddle.posX + gamePaddle.dimX, gamePaddle.posY + gamePaddle.dimY);
			gamePaddle.prev_posY = gamePaddle.posY;
			taskEXIT_CRITICAL();
		}
	}
}
