#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
/* Mode */
#define INT_DISABLE           0b11000000
#define INT_ENABLE            0b01000000
#define IRQ_MODE              0b10010
#define SVC_MODE              0b10011
#define TIMER_BASE            0xFF202000	
/* Memory */
#define A9_ONCHIP_END         0xFFFFFFFF
/* Cyclone V FPGA Device */
#define PS2_BASE              0xFF200100
/* Interrupt controller (GIC) CPU interface(s) */
#define MPCORE_GIC_CPUIF      0xFFFEC100    // PERIPH_BASE + 0x100
#define ICCICR                0x00          // offset to CPU interface control reg
#define ICCPMR                0x04          // offset to interrupt priority mask reg
#define ICCEOIR               0x10 	
#define ICCIAR                0x0C 	
/* Interrupt controller (GIC) distributor interface(s) */
#define MPCORE_GIC_DIST       0xFFFED000    // PERIPH_BASE + 0x1000
#define ICDDCR                0x00          // offset to distributor control reg
/* Keyboard related Variables */
#define PS2_IRQ 			  79            // Interrupt ID
#define PS2_L_ARROW           0x6B
#define PS2_R_ARROW           0x74
#define PS2_BACKSPACE         0x66
#define PS2_ENTER             0x5A
/* Game variables */
#define NO_TILE               -1
#define TILE_dimension        3

// configuring interrupts
void config_all_IRQ_interrupts(); // set all signals to configure interrupts
void set_A9_IRQ_stack(); // initiate the stack pointer for IRQ mode
void config_GIC(); // configure the general interrupt controller
void config_PS2(); // configure PS2 to generate interrupts
void disable_A9_interrupts(); //turn off interrupts
void enable_A9_interrupts(); // enable interrupts
void config_interrupts(int N, int CPU_target);
void shuffle();
void counter();
void config_interval_timer(); // configure Altera interval timer to generate

// Interrupt Service Routine
void PS2_ISR();
void interval_timer_ISR();

// VGA
void init_vga_buffer();
void wait_for_vsync();

// graphics
void draw_initial_game_tiles(); // draw initial configuration of tiles
void draw_tile(int position);

void draw_selected_tile_frame(bool is_erase);
int* get_png_of_tile(int num); // returns array of png corresponding to tile number
void drawing_png(int i, int j, int array[], int value);
void clear_screen();
void plot_pixel(int x, int y, short int line_color);
// animate the motion of tile moving from selected tile position -> no tile position
void animate_swap_tile();

// keyboard tile selections
void get_selectable_tiles(int* selectable_tiles, int* size, int* current_select_index);
bool is_tile_position_legal(int new_pos);
void select_new_selected_tile(int direction_offset);
void swap_tile();
void reset_selected_tile();
void drawing_png2(int i, int j, int array[], int value);

// game logic
void new_game_board(int array1[], int array2[]);
void check_game_status();

// debug; pass 16 for unused num
void display_on_hex(int num_a, int num_b, int num_c, int num_d, int num_e, int num_f);
	

volatile int pixel_buffer_start; // global variable

int game_tile_positions[9];
int gameNumber = 0;
int value = 0;
int count=0;
int no_tile_position = 8;
int selected_tile_position = 5;
bool game_over = false;

int win[];
int lose[];


int main(){
    
    config_all_IRQ_interrupts();

    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    /* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;
	clear_screen();
	draw_initial_game_tiles();
	
    while(1)
	counter();
    return 0;

}


// set all signals to configure interrupts
void config_all_IRQ_interrupts(){
    disable_A9_interrupts();
    set_A9_IRQ_stack();
    config_GIC();
	config_interval_timer();
    config_PS2();
    enable_A9_interrupts();
}


void config_interval_timer()
{
	volatile int * interval_timer_ptr =
	(int *)TIMER_BASE; // interal timer base address
	/* set the interval timer period for scrolling the HEX displays */
	int counter = 100000000; // 1/(100 MHz) x 5x10^6 = 50 msec
	*(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
	*(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;
	/* start interval timer, enable its interrupts */
	*(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1
}


void set_A9_IRQ_stack(void)
{
	int stack, mode;
	stack = A9_ONCHIP_END - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = INT_DISABLE | IRQ_MODE;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r"(stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = INT_DISABLE | SVC_MODE;
	asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}


// turn on interrupts in the ARM processor
void enable_A9_interrupts(){
    int status = INT_ENABLE | SVC_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}


// configure the Generic Interrupt Controller GIC
// code taken from DE1-SOC ARM Cortex A9 Document
void config_GIC(){
    int address;

    config_interrupts(PS2_IRQ, 1);

    *((int *)0xFFFED8C4) = 0x01000000;
	*((int *)0xFFFED118) = 0x00000080;
	/* configure the FPGA interval timer and KEYs interrupts */
	*((int *)0xFFFED848) = 0x00000101;
	*((int *)0xFFFED108) = 0x00000300;
	// Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all
	// priorities
	address = MPCORE_GIC_CPUIF + ICCPMR;
	*((int *)address) = 0xFFFF;
	// Set CPU Interface Control Register (ICCICR). Enable signaling of
	// interrupts
	address = MPCORE_GIC_CPUIF + ICCICR;
	*((int *)address) = 1;
	// Configure the Distributor Control Register (ICDDCR) to send pending
	// interrupts to CPUs
	address = MPCORE_GIC_DIST + ICDDCR;
	*((int *)address) = 1;
}


void __attribute__((interrupt)) __cs3_isr_irq(void)
{
	// Read the ICCIAR from the processor interface
	int address = MPCORE_GIC_CPUIF + ICCIAR;
	int interrupt_ID = *((int *)address);
	if (interrupt_ID ==72) // check if interrupt is from the Altera timer
		interval_timer_ISR();
	else if (interrupt_ID == 79)
       PS2_ISR();
	else
	while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	address = MPCORE_GIC_CPUIF + ICCEOIR;
	*((int *)address) = interrupt_ID;
	return;
}


// Define the remaining exception handlers
void __attribute__((interrupt)) __cs3_reset(void)
{
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_undef(void)
{
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_swi(void)
{
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_pabort(void)
{
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_dabort(void)
{
	while (1);
}
void __attribute__((interrupt)) __cs3_isr_fiq(void)
{
	while (1);
}


void interval_timer_ISR()
{
	volatile int * interval_timer_ptr = (int *)TIMER_BASE;	
	*(interval_timer_ptr) = 0; // Clear the interrupt
	if (!game_over){
        count++;
    }
	return;
}


// ISR for keyboard
void PS2_ISR(){
    volatile int* PS2_ptr = (int *) PS2_BASE;

    int PS2_data = *(PS2_ptr) & 0xFF;
    if (PS2_data == 0xF0) {
        display_on_hex(16,16,16,16,16,16);
        PS2_data = *(PS2_ptr) & 0xFF;
        if (PS2_data == PS2_ENTER)
        {
            swap_tile();
        } 
        else if (PS2_data == PS2_BACKSPACE)
        {
            shuffle();
        } 
    } else if (PS2_data == 0xE0) {
        display_on_hex(16,16,16,16,16,16);
        PS2_data = *(PS2_ptr) & 0xFF;
        if (PS2_data == 0xF0){
            PS2_data = *(PS2_ptr) & 0xFF;
            if (PS2_data == PS2_R_ARROW)
            {
                select_new_selected_tile(-1);
            } 
            else if (PS2_data == PS2_L_ARROW)
            {
                select_new_selected_tile(1);
            }
        }
    }	

    return;
}


void shuffle()
{
    if (game_over){
        game_over = false;
		
        clear_screen();
		count = 0;
    }
	count = 0;

    int game0[] = {2,7,3,NO_TILE,1,6,5,4,8};
    int game1[] = {4,NO_TILE,2,5,6,7,8,1,3};
    int game2[] = {3,4,NO_TILE,1,5,8,7,2,6};
    int game3[] = {1,2,3,4,5,NO_TILE,7,8,6};
	int game4[] = {4,3,NO_TILE,5,2,1,7,8,6};
	int game5[] = {2,3,5,1,NO_TILE,6,4,8,7};
	int game6[] = {2,6,NO_TILE,4,8,3,7,1,5};
	int game7[] = {1,3,6,8,NO_TILE,5,4,7,2};
	int game8[] = {NO_TILE,1,3,4,7,5,8,2,6};
	int game9[] = {NO_TILE,1,2,4,8,3,5,7,6};
	int game10[] ={2,3,8,NO_TILE,6,5,7,1,4};
	value = 0;
	if(gameNumber==0)
	{
		new_game_board(game_tile_positions,game0);
	}
  	 else if(gameNumber==1)
	{
		new_game_board(game_tile_positions,game1);
	}
	 else if(gameNumber==2)
	{
		new_game_board(game_tile_positions,game2);
	}
     else if(gameNumber==3)
	{
		new_game_board(game_tile_positions,game3);
	}
	 else if(gameNumber==4)
	{
		new_game_board(game_tile_positions,game4);
	}
	 else if(gameNumber==5)
	{
		new_game_board(game_tile_positions,game5);
	}
	 else if(gameNumber==6)
	{
		new_game_board(game_tile_positions,game6);
	}
	 else if(gameNumber==7)
	{
		new_game_board(game_tile_positions,game7);
	}
	 else if(gameNumber==8)
	{
		new_game_board(game_tile_positions,game8);
	}
	 else if(gameNumber==9)
	{
		new_game_board(game_tile_positions,game9);
	}
	else if(gameNumber==10)
	{
		new_game_board(game_tile_positions,game9);
	}
    // generate an automatic winning board for demo
    if (gameNumber == 10){
        new_game_board(game_tile_positions, game3);
        gameNumber = 0;
    } else {
        ++gameNumber;
    }
  	
    for (int k = 0; k < 9; ++k){
       draw_tile(k);
    }

    reset_selected_tile();

}


void new_game_board(int array1[], int array2[])
{
	for(int i=0;i<9;i++)
		{
			array1[i]=array2[i];
            if (array2[i] == NO_TILE){
                no_tile_position = i;
            }
		}
}


// swap tile at selected position with no tile position
void swap_tile(){

    // animate tile swapping
    animate_swap_tile();

    // swap selected tile and no tile positions
    game_tile_positions[no_tile_position] = game_tile_positions[selected_tile_position];
    game_tile_positions[selected_tile_position] = NO_TILE;

    // set global variable no tile position to selected position
    int temp = no_tile_position;
    no_tile_position = selected_tile_position;
    selected_tile_position = temp;
    draw_selected_tile_frame(false);
	check_game_status();
}


// reset selected tile and redraw the frame
void reset_selected_tile(){
    // set new selected tile
    int selectable_tiles[4];
    int temp1, temp2;
    get_selectable_tiles(selectable_tiles, &temp1, &temp2);
    selected_tile_position = selectable_tiles[0];
    // draw frame
    draw_selected_tile_frame(false);
}


// animate the motion of tile moving from selected tile position -> no tile position
void animate_swap_tile(){
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;

    int row_selected = selected_tile_position % 3;
    int col_selected = selected_tile_position / 3;
    int row_no_tile = no_tile_position % 3;
    int col_no_tile = no_tile_position / 3;

    // determine how much to move by
    int anim_x = 0;
    int anim_y = 0;
    if (row_no_tile == row_selected){
        if (col_no_tile > col_selected){
            anim_y = 15;
        } else if (col_no_tile < col_selected){
            anim_y = -15;
        }
    } else if (col_no_tile == col_selected){
        if (row_no_tile > row_selected){
            anim_x = 20;
        } else if (row_no_tile < row_selected){
            anim_x = -20;
        }
    }

    // current x and y values of selected tile
    int x_selected = 12 + row_selected*100;
    int y_selected = 12 + col_selected*75;
    // erase selected tile
    drawing_png(x_selected, y_selected, get_png_of_tile(NO_TILE), x_selected);
    x_selected += anim_x;
    y_selected += anim_y;

    while(1){
        // draw
        drawing_png(x_selected, y_selected, 
                    get_png_of_tile(game_tile_positions[selected_tile_position]),
                    x_selected);

        if (anim_x != 0 && (x_selected == 12 + row_no_tile *100)){
            break;
        } 
        if (anim_y != 0 && (y_selected == 12 + col_no_tile *75)){
            break;
        }

        wait_for_vsync();        
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer

        // erase
        drawing_png(x_selected, y_selected, get_png_of_tile(NO_TILE), x_selected);
        // move
        x_selected += anim_x;
        y_selected += anim_y;

    }
}


void check_game_status()
{
	int count=0;
	for(int i=0; i<9;i++)
	{
		if(game_tile_positions[i]==i+1)
		{
			count++;
		}
	}

	if(count==8)
	{
        game_over = true;
		clear_screen();
		drawing_png2(80,40,win,80);
	}
	
}


void counter()
{
	volatile int * HEX3_0_ptr = (int*) 0xFF200020;
    char seg7[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67, 0x77, 
					0x7c, 0x39, 0x5e, 0x79, 0x71};
    int value1;
	int value2;
	int value3;
	int inter;
	int inter2;
	int second;
	// int minute;
	while(count<=180)
	{
		second=((count % 3600) % 60);
		// minute=(second % 3600)/60;
		value1 = second%10;
		inter = second/10;
		value2 = inter%10;
		inter2 = count/60;
		value3= inter2;
		//value3 = value%1000;
		
		*(HEX3_0_ptr) = seg7[value1] | seg7[value2] << 8 | 
			seg7[value3] << 16;
		
	}
    game_over = true;
	count=0; 
	clear_screen();
	drawing_png2(80,40,lose,80);
}


void drawing_png2(int i, int j, int array[], int value)
{
	int W = 160;
	int H = 160;
	int initial = i;
    // check for NO_TILE
    if (array[0] == NO_TILE){
        for (int x = i; x < i+W; ++x){
            for (int y = j; y < j+H; ++y){
                plot_pixel(x, y, 0xFFFF);
            }
        }
    } else {
        for (int k = 0 ; k < W*H*2 - 1; k+= 2) {
            int red = ((array[k + 1] & 0xF8) >> 3) << 11;
            int green  = (((array[k] & 0xE0) >> 5)) | ((array[k+1] & 0x7) << 3) ;		
                
            int blue = (array[k] & 0x1f);			
            
            short int p = red | ( (green << 5) | blue);
            
            plot_pixel(i, j, p);
            
            i+=1;		  
            if (i == (W+value)) {
                i = initial;
                j+=1;
            }
        }
	}
}


void wait_for_vsync () {
    volatile int *pixel_ctrl_ptr = (int*) 0xFF203020;
    register int status;
    // make the request for the swap:
    *(pixel_ctrl_ptr) = 1; // writes 1 to the front buffer register, doesn’t change it, is a signal
    status = *(pixel_ctrl_ptr + 3); // as above, pointer arithmetic says 3
    while ( ( status & 0x01) != 0) 
    {
        status = *(pixel_ctrl_ptr +3);
    }
}


// selects new right or left tile, updtes selected tile position and draw frame around it
// direction_offset = -1 for left, +1 for right
void select_new_selected_tile(int direction_offset){
    int selectable_tiles[4]; // array of selectable tile position
    int selectable_tiles_num; // number of tiles selectable
    int current_select_index; // index of currently selected tile wrt selectable_tiles array

    get_selectable_tiles(selectable_tiles, &selectable_tiles_num, &current_select_index);
    int select_index = current_select_index + direction_offset;

    if (select_index < 0) {
        select_index = selectable_tiles_num-1;
    } if (select_index >= selectable_tiles_num) {
        select_index = 0;
    }

    // erase current frame
    draw_selected_tile_frame(true);

    // set new tile position and draw frame
    selected_tile_position = selectable_tiles[select_index];
    draw_selected_tile_frame(false);

}


// returns array of tile numbers that are selectable by users
// change the parameter selectale_tiles to the array
// and puts the number of selectable tiles into size
void get_selectable_tiles(int* selectable_tiles, int* size, int* current_select_index){
    int temp_ind = 0;
    int temp_tile_pos;

    // above
    temp_tile_pos = no_tile_position - TILE_dimension;
    if (is_tile_position_legal(temp_tile_pos)){
        selectable_tiles[temp_ind] = temp_tile_pos;
        if (temp_tile_pos == selected_tile_position){
            *current_select_index = temp_ind;
        }
        temp_ind += 1;
    } 

    // left
    temp_tile_pos = no_tile_position - 1;
    if (is_tile_position_legal(temp_tile_pos) && (temp_tile_pos / TILE_dimension == no_tile_position / TILE_dimension)){
        selectable_tiles[temp_ind] = temp_tile_pos;
        if (temp_tile_pos == selected_tile_position){
            *current_select_index = temp_ind;
        }
        temp_ind += 1;
    } 

    // below
    temp_tile_pos = no_tile_position + TILE_dimension;
    if (is_tile_position_legal(temp_tile_pos)){
        selectable_tiles[temp_ind] = temp_tile_pos;
        if (temp_tile_pos == selected_tile_position){
            *current_select_index = temp_ind;
        }
        temp_ind += 1;
    } 

    // right
    temp_tile_pos = no_tile_position + 1;
    if (is_tile_position_legal(temp_tile_pos) && (temp_tile_pos / TILE_dimension == no_tile_position / TILE_dimension)){
        selectable_tiles[temp_ind] = temp_tile_pos;       
        if (temp_tile_pos == selected_tile_position){
            *current_select_index = temp_ind;
        }
        temp_ind += 1; 
    } 
    *size = temp_ind;
}


// draw initial configuration of tiles
void draw_initial_game_tiles(){
   
   gameNumber = rand()%6;
    
    shuffle();

}



// draws tile at position 
void draw_tile(int position){
    int row = position % 3;
    int col = position / 3;
	
    drawing_png(12 + row*100, 12 + col*75, 
                        get_png_of_tile(game_tile_positions[TILE_dimension*col + row]),
                        12 + row*100);
}


// draw frame around selected tile
void draw_selected_tile_frame(bool is_erase){

    int row = selected_tile_position % 3;
    int col = selected_tile_position / 3;

    int start_pos_x = 12 + row*100;
    int start_pos_y = 12 + col*75;
    
    int frame_width_top = 5;
    int frame_width_side = 8;
    int width = 90;
    int height = 64;

    short int color = 0;
    if (is_erase){
        color = 0xFFFF;
        start_pos_x += 1;
        start_pos_y += 1;
        height -= 2;
        width -= 2;
    }

    // draw top and bottom frame
    for (int y_offset = 0; y_offset < frame_width_top; ++y_offset){
        for (int x_offset = 0; x_offset < width; ++x_offset){
            plot_pixel(start_pos_x + x_offset, start_pos_y + y_offset, color); // top frame
            plot_pixel(start_pos_x + x_offset, start_pos_y + y_offset + height - frame_width_top, color); // bottom frame
        }
    }

    // draw side frames
    for (int x_offset = 0; x_offset < frame_width_side; ++x_offset){
        for (int y_offset = frame_width_top; y_offset < height - frame_width_top; ++y_offset){
            plot_pixel(start_pos_x + x_offset, start_pos_y + y_offset, color); // left frame
            plot_pixel(start_pos_x + x_offset + width - frame_width_side, start_pos_y + y_offset, color); // right frame
        }
    }
}


// draws a png tile 1-8
void drawing_png(int i, int j, int array[], int value)
{
	int W = 90;
	int H = 64;
	int initial = i;
    // check for NO_TILE
    if (array[0] == NO_TILE){
        for (int x = i; x < i+W; ++x){
            for (int y = j; y < j+H; ++y){
                plot_pixel(x, y, 0xFFFF);
            }
        }
    } else {
        for (int k = 0 ; k < W*H*2 - 1; k+= 2) {
            int red = ((array[k + 1] & 0xF8) >> 3) << 11;
            int green  = (((array[k] & 0xE0) >> 5)) | ((array[k+1] & 0x7) << 3) ;		
                
            int blue = (array[k] & 0x1f);			
            
            short int p = red | ( (green << 5) | blue);
            
            plot_pixel(i, j, p);
            
            i+=1;		  
            if (i == (W+value)) {
                i = initial;
                j+=1;
            }
        }
	}
}


// check if specific position on the board is legal
bool is_tile_position_legal(int new_pos){
    if (new_pos < 0) {
        return false;
    } else if (new_pos >= TILE_dimension*TILE_dimension){
        return false;
    }
    return true;
}


void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}


// clears screen by writing white
void clear_screen(){
    
	memset((short int*) pixel_buffer_start,  0xffff, 245760 ); 
}


// set front and back buffers of VGA
void init_vga_buffer(){
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;

    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    // clear_screen(); // pixel_buffer_start points to the pixel buffer
    /* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
}


// configure PS2 to reset & generate interrupts
void config_PS2(){
    volatile int* PS2_ptr = (int*) PS2_BASE;

    *(PS2_ptr) = 0xFF; // reset
    volatile int* PS2_ptr_ctrl = (int*) 0xFF200104;
    *(PS2_ptr_ctrl) = 1; // set RE to 1 to generate interrupts
}


// turn off interrupts in the ARM processor
void disable_A9_interrupts(){
    int status = INT_DISABLE | SVC_MODE;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}


// configure ICDISERn and ICDIPTRn
// code taken from Using the ARM GIC document
void config_interrupts(int N, int CPU_target){
    int reg_offset, index, value, address;

    reg_offset = (N >> 3) & 0xFFFFFFFC;
    index = N & 0x1F;
    value = 0x1 << index;
    address = 0xFFFED100 + reg_offset;

    *(int*) address |= value;

    reg_offset = (N & 0xFFFFFFFC);
    index = N & 0x3;
    address = 0xFFFED800 + reg_offset + index;

    *(char *) address = (char)CPU_target;
}


int convert_num(int num){
    if (num < 0 || num >16){
        return 16;
    }
    return num;
}


void display_on_hex(int num_a, int num_b, int num_c, int num_d, int num_e, int num_f){
    volatile int* HEX_3_0_ptr = (int *) 0xFF200020;

    unsigned char seven_seg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
                                 0x7f, 0x67, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x0};
    num_a = convert_num(num_a);
    num_b = convert_num(num_b);
    num_c = convert_num(num_c);
    num_d = convert_num(num_d);
    num_e = convert_num(num_e);
    num_f = convert_num(num_f);

    *HEX_3_0_ptr = (seven_seg[num_d] << 24) | (seven_seg[num_c] << 16) | (seven_seg[num_b] << 8) | (seven_seg[num_a]);

    volatile int* HEX_5_4_ptr = HEX_3_0_ptr + 4;
    *HEX_5_4_ptr = seven_seg[num_f] << 8 | seven_seg[num_e];

}
