# 15-game-puzzle 


15 Puzzle is a game where user moves tiles to rearrange them in a particular order.

<br>

This game is programmed in C Programming language, and operates on DE1-SOC computer and ARM processor.

<br>

### Compiling the Code
1. Go to <a href="https://cpulator.01xz.net/?sys=arm-de1soc">CPULator</a> 
2. Select C as the Programming Language
3. Copy all the codes in <b>15-puzzle-game.c</b> and paste them into the editor
4. Compile and Load (F5), then press Continue (F3) 

<br>

### Display
* <b>VGA</b>: 8 tiles numbered 1-8 will be displayed in a 3x3 block in random order
* <b>Hex</b>: the timer value is displayed on hex, counting up (time limit is 3 minutes)

<br>

### How to Play
- Type PS2 keyboard <b>Right Arrow</b> or <b>Left Arrow</b> to select the tile you want to move.
- Right Arrow selects clockwise, Left Arrow selects counterclockwise
- The selected tile is indicated with a thick black frame
- Type PS2 <b>Enter</b> key to move the tile
- Selected tile slides to the empty spot
- Repeat until the tiles are sorted in ascending order (shown below)

    <br>


    |  |  |  |
    | --- | --- | --- | 
    | 1 | 2 | 3 |
    | 4 | 5 | 6 |
    | 7 | 8 |   |

    <br>


- If user is able to arrange the tiles within the time limit, “You Win” appears on VGA
- If time limit is exceeded, “You Lose” appears on VGA
- Type PS2 <b>Backspace</b> key is used to restart the game (after a game ends) or shuffle
the tile arran

<br><br>


### Attribution Table
| Name | Work Done | Relative % <br>Work Done |
| - | - | - |
| Ruqhia | <ul><li>configured timer interrupts and ISR for timer</li><li>added images for game board, and win/lose page</li><li>implemented shuffling option</li></ul> | 50
| Nancy | <ul><li>configured PS2 interrupts and ISR for PS2</li><li>implemented tile selection and swapping logics</li><li>implemented animation for tile swapping</li></ul> | 50
