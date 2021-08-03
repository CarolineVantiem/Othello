/********************
 * Includes/Defines *
 ********************/
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/rwsem.h>
#include <asm/uaccess.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/miscdevice.h>

#define ROWS		 8
#define COLUMNS		 8

#define START_3		 3
#define START_4		 4
#define OCCUPIED	 2

#define BOARD_SIZE	 64
#define BOARD_MAX_CMD	 55
#define BOARD_SIZE_RET	 67

#define FAIL		 1
#define ILL_CMD		-1
#define IMP_FORM	-2
#define	NOT_LEGAL	-1

#define X		 1
#define O		 0
#define X_CMD		 88
#define O_CMD		 79
#define	SPACE		 32
#define X_CHAR		 'X'
#define O_CHAR		 'O'
#define END_CMD		 10
#define NUM_CMD		 48
#define EMPTY_CHAR	 '-'

#define OK		 4
#define WIN		 5
#define TIE		 5 
#define OOT		 5
#define LOSE		 6
#define PASS		 6
#define UNKCMD		 8 
#define INVFMT		 8
#define NOGAME		 8
#define ILLMOVE		 9

#define ZERO		 48
#define ONE		 49
#define TWO		 50
#define THREE		 51
#define FOUR		 52
#define FIVE		 53
#define SIX		 54
#define SEVEN		 55

#define ZERO_ARG	 0
#define FIRST_ARG	 1
#define SECOND_ARG	 2
#define THIRD_ARG	 3
#define	FOURTH_ARG	 4
#define FIFTH_ARG	 5
#define SIXTH_ARG	 6

#define DEVICE_NAME "reversi"


/***********
 * Globals *
 ***********/
static char *buf;

static int p2;
static int cpu;
static int user;
static int turn;
static int game = 0;	
static int mode = 0; 	/* track two-player reversi */
static int game_over;

static char p2_char;
static char cpu_char;
static char user_char;

static unsigned int col;
static unsigned int row;

static char turned_piece;
static char flipping_piece;

static char board[ROWS][COLUMNS];
static char p2_board[ROWS][COLUMNS];
static char cpu_board[ROWS][COLUMNS];
static char user_board[ROWS][COLUMNS];
static char board_return[BOARD_SIZE_RET];

/*
static DECLARE_RWSEM(lock);
*/


/***************
 * Error Codes *
 ***************/
static char ok[OK] = "OK\n";
static char win[WIN] = "WIN\n";
static char tie[TIE] = "TIE\n";
static char oot[OOT] = "OOT\n";
static char pass[PASS] = "PASS\n";
static char lose[LOSE] = "LOSE\n";
static char unkcmd[UNKCMD] = "UNKCMD\n";
static char invfmt[INVFMT] = "INVFMT\n";
static char nogame[NOGAME] = "NOGAME\n";
static char illmove[ILLMOVE] = "ILLMOVE\n";


/***********************
 * Function Prototypes *
 ***********************/
static int 	__init reversi_init(void);
static void	__exit reversi_exit(void);
static int 	reversi_open(struct inode *node, struct file *fptr);
static int	reversi_release(struct inode *node, struct file *fptr);
static ssize_t	reversi_write(struct file *fptr, const char *buffer, size_t len, loff_t *offset);
static ssize_t 	reversi_read(struct file *fptr, char __user *buffer, size_t len, loff_t *offset);


/********************
 * Helper Functions *
 ********************/
static int	check_command(void);			/* checks command validity */
static int	setup_reversi(void);			/* sets up reversi board */
static int	populate_gameboard(void);		/* populate returned gameboard */
static int	end_game(char array[ROWS][COLUMNS]);	/* checks if game is over */
static int	check_move(char array[ROWS][COLUMNS]);	/* makes a move/checks valid */
static int	setup_move(char array[ROWS][COLUMNS]); 	/* sets up move for players */

/*
	Check surrounding squares
 	  	1    2    3
	  	8    x    4
	  	7    6    5
*/
static int	dir_1(char array[ROWS][COLUMNS]);			/* check diagonal up left */
static int	dir_2(char array[ROWS][COLUMNS]);			/* check up */
static int	dir_3(char array[ROWS][COLUMNS]);			/* check diagonal up right */
static int	dir_4(char array[ROWS][COLUMNS]);			/* check right */
static int	dir_5(char array[ROWS][COLUMNS]);			/* check diagonal down right */
static int	dir_6(char array[ROWS][COLUMNS]);			/* check down */
static int	dir_7(char array[ROWS][COLUMNS]);			/* check diagonal down left */
static int	dir_8(char array[ROWS][COLUMNS]);   			/* check left */

/***********
 * Structs *
 ***********/
static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= reversi_open,
	.read		= reversi_read,
	.write		= reversi_write,
	.release	= reversi_release,

};

struct miscdevice reversi_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name 	= DEVICE_NAME,
	.fops	= &fops,
	.mode 	= 0666,
};


/****************************
 * File Operation Functions *
 ****************************/
static int reversi_open(struct inode *node, struct file *fptr)
{
	return 0;
}

static int reversi_release(struct inode *node, struct file *fptr)
{
	return 0;
}

static ssize_t reversi_write(struct file *fptr, const char __user *buffer, size_t len, loff_t *offset)
{
	int retval = 0;
	
	/*down_write(&lock);*/

	if (!access_ok(buffer, len))
		return -EFAULT;

	/* allocate & copy data */
	buf = kmalloc(len, GFP_KERNEL);
	retval = copy_from_user(buf, buffer, len);
	buf[len] = '\0';

	if (retval < 0)
		return -EINVAL;
	
	/*up_write(&lock);*/

	return len;
}

static ssize_t reversi_read(struct file *fptr, char __user *buffer, size_t len, loff_t *offset)
{
	int cmd = 0;
	int end = 0;
	int size = 0;
	int move = 0;
	int check = 0;
	int retval = 0;
	
	/*down_write(&lock);*/

	/* illegal/improperly formatted commands */
	check = check_command();
	if (check == ILL_CMD) {
    		retval = copy_to_user(buffer, unkcmd,  UNKCMD);
	    	if (retval < 0)
	    		return -EINVAL;
	    	return UNKCMD;
	}

	else if (check == IMP_FORM) {
    		retval = copy_to_user(buffer, invfmt, INVFMT);
	   	 if (retval < 0)
	  	  	return -EINVAL;
	  	  return INVFMT;
	}
	
	
	/* execute reversi game commands */
	cmd = buf[FIRST_ARG];
	if (cmd == FIVE)
		cmd = ZERO;

	switch(cmd) {

		/* command 00/05 X/O */
		case ZERO:
			setup_reversi();
			retval = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval < 0)
				return -EINVAL;
			break;

		/* command 01 */
		case ONE:
			if (game == 0) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			populate_gameboard();
			retval = copy_to_user(buffer, board_return, BOARD_SIZE_RET);
			size = BOARD_SIZE_RET;
			if (retval < 0)
				return -EINVAL;
			break;

		/* command 02 C R */
		case TWO:
			row = buf[FIFTH_ARG] - NUM_CMD;
			col = buf[THIRD_ARG] - NUM_CMD;

			if (game == 0 || game_over == 1) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			if (turn != user) {
				retval = copy_to_user(buffer, oot, OOT);
				if (retval < 0)
					return -EINVAL;
				return OOT;
			}

			move = setup_move(board);
			if (move == 1 || move == 2) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if(retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			end = end_game(user_board);
			if (end == 0) {
				retval = copy_to_user(buffer, win, WIN);
				if (retval < 0)
					return -EINVAL;
				return WIN;
			}
			else if (end == 1) {
				retval = copy_to_user(buffer, lose, LOSE);
				if (retval < 0)
					return -EINVAL;
				return LOSE;
			}
			else if (end == 2) {
				retval = copy_to_user(buffer, tie, TIE);
				if (retval < 0)
					return -EINVAL;
				return TIE;
			}

			retval = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval < 0)
				return -EINVAL;

			if (mode == 1)
				turn = p2;
			else if (mode == 0)
				turn = cpu;

			break;

		/* command 03 */
		case THREE:
			if (game == 0 || game_over == 1) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			if (mode == 1) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if (retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			if (turn != cpu) {
				retval = copy_to_user(buffer, oot, OOT);
				if (retval < 0)
					return -EINVAL;

				return OOT;
			}
	
			move = check_move(board);
			if (move ==  FAIL) {
				turn = user;
				retval = copy_to_user(buffer, pass, PASS);
				if (retval < 0)
					return -EINVAL;
				return PASS;
			}

			end = end_game(cpu_board);
    			if (end == 0) {
				retval = copy_to_user(buffer, win, WIN);
				if (retval < 0)
					return -EINVAL;
				return WIN;
			}
			else if (end == 1) {
				retval = copy_to_user(buffer, lose, LOSE);
				if (retval < 0)
					return -EINVAL;
				return LOSE;
			}
			else if (end == 2) {
				retval = copy_to_user(buffer, tie, TIE);
				if (retval < 0)
					return -EINVAL;
				return TIE;
			}

			retval = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval < 0)
				return -EINVAL;

			turn = user;
			break;

		/* command 04 */
		case FOUR:
			if (game == 0 || game_over == 1) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			if (turn != user) {
				retval = copy_to_user(buffer, oot, OOT);
				if (retval < 0)
					return -EINVAL;
				return OOT;
			}
				
			move = check_move(user_board);
			if (move == 0) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if (retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			end = end_game(cpu_board);
			if (end == 0) {
				retval = copy_to_user(buffer, win, WIN);
				if (retval  < 0)
					return -EINVAL;
				return WIN;
			}
			else if (end ==  1) {
				retval = copy_to_user(buffer, lose, LOSE);
				if (retval < 0)
					return -EINVAL;
				return LOSE;
			}
			else if (end == 2)  {
				retval  = copy_to_user(buffer, tie, TIE);
				if (retval < 0)
					return -EINVAL;
				return TIE;
			}
			
			retval  = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval <  0)
				return -EINVAL;

			if (mode == 1)
				turn = p2;
			else if (mode == 0)
				turn = cpu;
			break;
		
		/* command 06 */
		case SIX:
			if (game == 0 || game_over == 1) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			if (mode == 0) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if (retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			row = buf[FIFTH_ARG] - NUM_CMD;
			col = buf[THIRD_ARG] - NUM_CMD;

			if (turn != p2) {
				retval = copy_to_user(buffer, oot, OOT);
				if (retval < 0)
					return -EINVAL;
				return OOT;
			}

			move = setup_move(board);
			if (move == 1 || move == 2) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if(retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			end = end_game(p2_board);
			if (end == 0) {
				retval = copy_to_user(buffer, win, WIN);
				if (retval < 0)
					return -EINVAL;
				return WIN;
			}
			else if (end == 1) {
				retval = copy_to_user(buffer, lose, LOSE);
				if (retval < 0)
					return -EINVAL;
				return LOSE;
			}
			else if (end == 2) {
				retval = copy_to_user(buffer, tie, TIE);
				if (retval < 0)
					return -EINVAL;
				return TIE;
			}

			retval = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval < 0)
				return -EINVAL;

			turn = user;
			break;

		/* command 07 */
		case SEVEN:
			if (game == 0 || game_over == 1) {
				retval = copy_to_user(buffer, nogame, NOGAME);
				if (retval < 0)
					return -EINVAL;
				return NOGAME;
			}

			if (mode == 0) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if (retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			if (turn != p2) {
				retval = copy_to_user(buffer, oot, OOT);
				if (retval < 0)
					return -EINVAL;
				return OOT;
			}

			move = check_move(p2_board);
			if (move == 0) {
				retval = copy_to_user(buffer, illmove, ILLMOVE);
				if (retval < 0)
					return -EINVAL;
				return ILLMOVE;
			}

			end = end_game(user_board);
			if (end == 0)  {
				retval  = copy_to_user(buffer, win, WIN);
				if (retval < 0)
					return -EINVAL;
				return WIN;
			}
			else if (end == 1)  {
				retval  =  copy_to_user(buffer, lose, LOSE);
				if (retval < 0)
					return -EINVAL;
				return LOSE;
			}
			else if (end == 2) {
				retval  =  copy_to_user(buffer, tie, TIE);
				if (retval < 0)
					return -EINVAL;
				return TIE;
			}

			retval = copy_to_user(buffer, ok, OK);
			size = OK;
			if (retval  < 0)
				return -EINVAL;

			turn = user;
			break;
	}
	
	/*up_write(&lock);*/

	return size;
}


/*********************
 * Starter Functions *
 *********************/
static int __init reversi_init(void)
{
	int misc_num = 0;

	/* create misc device */
	misc_num = misc_register(&reversi_misc);

	if (misc_num < 0)
		return -ENODEV;

	return 0;
}

static void __exit reversi_exit(void)
{
	/* clean up driver */
	kfree(buf);
	misc_deregister(&reversi_misc);
}


/********************
 * Helper Functions *
 ********************/
static int check_command(void)
{
	/* validate game commands */
	int cmd = 0;

	if (buf[ZERO_ARG] != ZERO)
		return ILL_CMD;

	if (buf[FIRST_ARG] > SEVEN || buf[FIRST_ARG] < ZERO)
		return ILL_CMD;

	cmd = buf[FIRST_ARG];
	if (cmd == SIX)
		cmd = TWO;
	
	/* validate commands follow input form */
	switch(cmd) {

		case ZERO:
			if (buf[SECOND_ARG] != SPACE)
				return IMP_FORM;

			if (buf[THIRD_ARG] != X_CMD && buf[THIRD_ARG] != O_CMD)
				return IMP_FORM;

			if (buf[FOURTH_ARG] != END_CMD)
				return IMP_FORM;
			break;

		case TWO:
			if (buf[SECOND_ARG] != SPACE)
				return IMP_FORM;

			if (buf[SIXTH_ARG]!= END_CMD)
				return IMP_FORM;

			if (buf[FOURTH_ARG] != SPACE)
			       return IMP_FORM;

			if (buf[THIRD_ARG] > BOARD_MAX_CMD)
				return ILL_CMD;

			if (buf[FIFTH_ARG] > BOARD_MAX_CMD)
				return ILL_CMD;
			break;

		default:
			if (buf[SECOND_ARG] != END_CMD)
				return IMP_FORM;
	}

	return 0;
}

static int setup_reversi(void)
{
	/* setup board/pieces */
	int i = 0;
	int j = 0;
	int piece = 0;
	int game_mode = 0;

	game_mode = buf[FIRST_ARG];

	/* setup for 1player or 2player reversi */
	switch(game_mode) {
		case ZERO:
			mode = 0;
			piece = buf[THIRD_ARG];
			if (piece == X_CMD) {
				user = X;
				user_char = X_CHAR;

				cpu = O;
				cpu_char = O_CHAR;
			}
			else if (piece == O_CMD) {
				user = O;
				user_char = O_CHAR;

				cpu = X;
				cpu_char = X_CHAR;
			}
			break;

		case FIVE:
			mode = 1;
			user = X;
			user_char = X_CHAR;

			p2 = O;
			p2_char = O_CHAR;
			break;

		default:
			return 0;
	}

	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++) {
			board[i][j] = '-';
		}
	}

	/* starting X pieces */
	board[START_3][START_3] = O_CHAR;
	board[START_4][START_4] = O_CHAR;

	/* starting O pieces */
	board[START_3][START_4] = X_CHAR;
	board[START_4][START_3] = X_CHAR;

	/* game started/set turn */
	game = 1;
	turn = 1;

	return 0;
}

static int populate_gameboard(void)
{

	int i = 0;
	int j = 0;
	int index = 0;

	/* populate return board from actual board */
	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++) {
			board_return[index] = board[i][j];
			index++;
		}
	}

	/* add remaining chars */
	board_return[64] = '\t';

	if (turn == O) {
		board_return[65] = O_CHAR;
	}
	else if (turn == X) {
		board_return[65] = X_CHAR;
	}

	board_return[66] = '\n';

	return 0;
}

/* setup and makes a move for any player
   return 0 on success
   return 1 on fail
   return 2 on occupied
*/
static int setup_move(char array[ROWS][COLUMNS])
{
	int dir1 = 0;
	int dir2 = 0;
	int dir3 = 0;
	int dir4 = 0;
	int dir5 = 0;
	int dir6 = 0;
	int dir7 = 0;
	int dir8 = 0;
	int check = 0;
	int legal_moves = 0;

	if (turn == user) {
		turned_piece = p2_char;
		flipping_piece = user_char;
	}

	else if (turn == p2) {
		turned_piece = user_char;
		flipping_piece = p2_char;
	}

	if (mode == 0) {

		if (turn == cpu) {
			turned_piece = user_char;
			flipping_piece = cpu_char;
		}

		else if (turn == user) {
			turned_piece = cpu_char;
			flipping_piece = user_char;
		}
	}
	
	if (board[row][col] == X_CHAR || board[row][col] == O_CHAR)
		return OCCUPIED;
	
	/* corner 1 */
	if (row == 0 && col == 0) {
		check = 1;
		dir4 = dir_4(array);
		dir5 = dir_5(array);
		dir6 = dir_6(array);

		legal_moves = dir4+dir5+dir6;
	}

	/* corner 2 */
	else if (row == 0 && col == 7) {
		check = 1;
		dir6 = dir_6(array);
		dir7 = dir_7(array);
		dir8 = dir_8(array);

		legal_moves = dir6+dir7+dir8;
	}

	/* corner 3 */
	else if (row == 7 && col == 7) {
		check = 1;
		dir1 = dir_1(array);
		dir2 = dir_2(array);
		dir8 = dir_8(array);

		legal_moves = dir1+dir2+dir8;
	}

	/* corner 4 */
	else if (row == 7 && col == 0) {
		check = 1;
        	dir2 = dir_2(array);
		dir3 = dir_3(array);
		dir4 = dir_4(array);

		legal_moves = dir2+dir3+dir4;
	}

	/* edge 1 */
	else if (row == 0) {
		check = 2;
		dir4 = dir_4(array);
		dir5 = dir_5(array);
		dir6 = dir_6(array);
		dir7 = dir_7(array);
		dir8 = dir_8(array);

		legal_moves = dir4+dir5+dir6+dir7+dir8;
	}

	/* edge 2 */
	else if (col == 7) {
		check = 2;
		dir1 = dir_1(array);
		dir2 = dir_2(array);
		dir6 = dir_6(array);
		dir7 = dir_7(array);
		dir8 = dir_8(array);
		
		legal_moves = dir1+dir2+dir6+dir7+dir8;
	}	

	/* edge 3 */
	else if (row == 7) {
		check = 2;
		dir1 = dir_1(array);
		dir2 = dir_2(array); 
		dir3 = dir_3(array);
		dir4 = dir_4(array);
		dir8 = dir_8(array);

		legal_moves = dir1+dir2+dir3+dir4+dir8;
	}

	/* edge 4 */
	else if (col == 0) {
		check = 2;
      		dir2 = dir_2(array);
		dir3 = dir_3(array);
		dir4 = dir_4(array);
		dir5 = dir_5(array);
		dir6 = dir_6(array);
	
		legal_moves = dir2+dir3+dir4+dir5+dir6;

	}

	else {
		check = 3;
		dir1 = dir_1(array);
		dir2 = dir_2(array);
		dir3 = dir_3(array);
		dir4 = dir_4(array);
		dir5 = dir_5(array);
		dir6 = dir_6(array);
		dir7 = dir_7(array);
		dir8 = dir_8(array);
	
		legal_moves = dir1+dir2+dir3+dir4+dir5+dir6+dir7+dir8;
	}

	/* not a legal move at row/col */
	switch(check) {
		case 1:
			if (legal_moves == 3)
				return FAIL;
			break;
		case 2:
			if (legal_moves == 5)
				return FAIL;
			break;
		case 3:
			if (legal_moves == 8)
				return FAIL;
	}

	return 0;
}


/*******************************
 * Reversi Direction Functions *
 *******************************/
/*	1 	2 	3
 	8	x	4
 	7	6	5
*/
static int dir_1(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 0;
	int found = 0;
	int found_spot = 0;

	if (col > row)
		stop = row;
	if (row >= col)
		stop = col;

	if (array[row-1][col-1] == turned_piece) {

		for (i = 1; i <= stop; i++) {
			if (array[row-i][col-i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}

			else if (array[row-i][col-i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row-i][col-i] = flipping_piece;
	
		return 0;
	}

	return FAIL;
}


static int dir_2(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 0;
	int found = 0;
	int found_spot = 0;

	stop = row;

	if (array[row-1][col] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row-i][col] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row-i][col] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row-i][col] = flipping_piece;
	
		return 0;
	}

	return FAIL;
}

static int dir_3(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 0;
	int found = 0;
	int found_spot = 0;

	if (row+col <= 7)
		stop = row;
	if (row+col > 7)
		stop = 7 - col;

	if (array[row-1][col+1] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row-i][col+i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row-i][col+i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row-i][col+i] = flipping_piece;
	
		return 0;
	}

	return FAIL;
}

static int dir_4(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 7-col;
	int found = 0;
	int found_spot = 0;

	if (array[row][col+1] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row][col+i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row][col+i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row][col+i] = flipping_piece;
	
		return 0;
	}

	return FAIL;
}

static int dir_5(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 0;
	int found = 0;
	int found_spot = 0;

	if (row >= col)
		stop = 7-row;
	if (row < col)
		stop = 7-col;

	if (array[row+1][col+1] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row+i][col+i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}

			else if (array[row+i][col+i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}


	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row+i][col+i] = flipping_piece;
	
		return 0;
	}

	return FAIL;
}

static int dir_6(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 7-row;
	int found = 0;
	int found_spot = 0;

	if (array[row+1][col] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row+i][col] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row+i][col] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row+i][col] = flipping_piece;
		
		return 0;
	}

	return FAIL;
}

static int dir_7(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = 0;
	int found = 0;
	int found_spot = 0;

	if (row+col <= 7)
		stop = col;

	if (row+col > 7)
		stop = 7-row;

	if (array[row+1][col-1] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row+i][col-i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row+i][col-i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row+i][col-i] = flipping_piece;
		
		return 0;
	}

	return FAIL;
}

static int dir_8(char array[ROWS][COLUMNS])
{
	int i = 0;
	int stop = col;
	int found = 0;
	int found_spot = 0;

	if (array[row][col-1] == turned_piece) {
		for (i = 1; i <= stop; i++) {
			if (array[row][col-i] == flipping_piece) {
				found_spot = i;
				found = 1;
				i = stop;
			}
			else if (array[row][col-i] == EMPTY_CHAR) {
				found = 0;
				i = stop;
			}
		}
	}

	if (found == 1) {
		array[row][col] = flipping_piece;

		for (i = 1; i <= found_spot; i++)
			array[row][col-i] = flipping_piece;
		
		return 0;
	}

	return FAIL;
}


/****************************
 * Reversi Helper Functions *
 ****************************/
/* checks for any valid moves/makes a move for the cpu
   return 0 on success
   return 1 if no valid move
*/
static int check_move(char array[ROWS][COLUMNS])
{	
	int i = 0;
	int j = 0;
	int legal_moves = 0;

	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++)
			array[i][j] = board[i][j];
	}
	
	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++) {
			row = i;
			col = j;

			legal_moves = 0;
			legal_moves = setup_move(array);

			if (legal_moves == 0) {
				i = ROWS;
				j = COLUMNS;
				return 0;
			}
		}
	}
	
	/* no legal move found */
	return FAIL;
}

/* calculates if game is over
   return 0 win
   return 1 lose
   return 2 tie

   return 3 not game over
*/
static int end_game(char array[ROWS][COLUMNS])
{
	int i;
	int j;
	int cpu_count = 0;
	int cpu_valid = 0;
	int user_count = 0;
	int user_valid = 0;
	int temp_turn = turn;
	
	char piece;

	turn = cpu;
	piece = cpu_char;
	cpu_valid = check_move(cpu_board);
	
	if (mode == 1) {
		turn = p2;
		piece = p2_char;
		cpu_valid = check_move(p2_board);
	}

	for (i = 0; i < ROWS; i++) {
		for (j = 0; j < COLUMNS; j++) {
			if (board[i][j] == user_char)
				user_count++;

			else if (board[i][j] == piece)
				cpu_count++;
		}
	}

	turn = user;
	user_valid = check_move(user_board);

	if ( (cpu_valid == 1 && user_valid == 1) || (user_count + cpu_count == 64)) {
		game_over = 1;
		if (user_count > cpu_count)
			return 0;
		else if (user_count < cpu_count)
			return 1;
		else if (user_count == cpu_count)
			return 2;
	}

	turn = temp_turn;
	return 3;

}


/******************
 * Macros/Modules *
 ******************/
module_init(reversi_init);
module_exit(reversi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caroline Vantiem");
MODULE_DESCRIPTION("Reversi character device driver");
