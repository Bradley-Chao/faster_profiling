
/* Name: main.c
 * Purpose: main executes a virtual machine program which reads in a user file
 * and executes 32-bit word instructions
 * By: Bradley Chao and Matthew Soto
 * Date: 11/16/2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>

#define CONDITIONAL_MOVE 0
#define SEGMENTED_LOAD 1
#define SEGMENTED_STORE 2
#define ADDITION 3
#define MULTIPLICATION 4
#define DIVISION 5
#define BITWISE_NAND 6
#define HALT 7
#define MAP_SEGMENT 8
#define UNMAP_SEGMENT 9
#define OUTPUT 10
#define INPUT 11
#define LOAD_PROGRAM 12
#define LOAD_VALUE 13

/*************************************************************************
                        Start Universal Machine Module 
*************************************************************************/
typedef uint32_t UM_instruction;

typedef struct universal_machine {
        uint32_t registers[8]; 
        uint32_t program_counter;

        /* C-Array with first 32-bit integer as size */
        uint32_t *unmapped_IDs;
        uint32_t num_IDs;
        uint32_t ID_arr_size;
        
        /* contiguous block of 64-bit addresses of queues which represent segments of 32-bit word instructions */
        uint32_t **segments;
        uint32_t segment_arr_size;
        uint32_t total_seg_space;

} *universal_machine;

universal_machine new_UM(uint32_t *program_instructions)
{
        universal_machine UM = malloc(sizeof(*UM));

        for (size_t i = 0; i < 8; i++) {
                UM->registers[i] = 0;
        }

        UM->program_counter = 0;

        UM->unmapped_IDs = malloc(1 * sizeof(uint32_t));
        UM->num_IDs = 0;
        UM->ID_arr_size = 1;

        UM->segments = malloc(1 * sizeof(uint32_t *));
        UM->segment_arr_size = 1;

        UM->total_seg_space = 1;

        /* Store pointer to first word instruction in segment zero */
        UM->segments[0] = program_instructions;

        return UM;
}

void free_UM(universal_machine *UM)
{
        /* Free malloc'd 32-bit instruction segments */

        uint32_t **spine = (*UM)->segments;

                
        for (size_t i = 0; i < (*UM)->total_seg_space; i++)
                free(spine[i]);

        free(spine);        

        /* Free unmapped IDs */
        free((*UM)->unmapped_IDs);

        /* Frees malloced pointer to the UM struct */
        free(*UM);
}

inline uint32_t map_segment(universal_machine UM, uint32_t num_words)
{
        /* Allocate (num_words + 1) * sizeof(32) bytes with words = 0 */
        uint32_t *new_segment = calloc(num_words + 1, sizeof(uint32_t));
        assert(new_segment);

        /* First elem stores the number of words */
        new_segment[0] = num_words;
        
        /* Case 1: If there are no unmapped IDs */
        if (UM->num_IDs == 0) {
                /* Check whether realloc is necessary for segments spine */
                if (UM->total_seg_space == UM->segment_arr_size) {
                        uint32_t bigger_arr_size = UM->segment_arr_size * 2;
                        UM->segments = realloc(UM->segments, bigger_arr_size * sizeof(uint32_t *));
                        assert(UM->segments);
                        UM->segment_arr_size = bigger_arr_size;
                }

                UM->segments[UM->total_seg_space] = new_segment;

                UM->total_seg_space++;

                return UM->total_seg_space - 1;
        }
        /* Case 2: There are unmapped IDs available for use */
        else {
                /* Back most element of array / top of stack */
                uint32_t available_ID = UM->unmapped_IDs[UM->num_IDs - 1];
                UM->num_IDs--;

                /* Free data that has been there */
                uint32_t *to_unmap = UM->segments[available_ID];
                free(to_unmap);

                UM->segments[available_ID] = new_segment;

                return available_ID;
        }
}

/* This function purely makes the ID available does not free data */
inline void unmap_segment(universal_machine UM, uint32_t segment_ID)
{
        /* Add the new ID to the ID C-array */
        if (UM->num_IDs == UM->ID_arr_size) {
                uint32_t bigger_arr_size = UM->ID_arr_size * 2;
                UM->unmapped_IDs = realloc(UM->unmapped_IDs, bigger_arr_size * sizeof(uint32_t));
                assert(UM->unmapped_IDs);
                UM->ID_arr_size = bigger_arr_size;
        }

        /* Push the newly available ID to the top of the stack */
        UM->unmapped_IDs[UM->num_IDs] = segment_ID;

        /* Update number of IDs and number of segments */
        UM->num_IDs++;
}

/*************************************************************************
                        End Universal Machine Module 
*************************************************************************/

/*************************************************************************
                        Start Instruction Set Module 
*************************************************************************/

typedef unsigned UM_Reg;

#define mod_limit 4294967296;

/* Name: conditional_move
*  Purpose: if $r[C] != 0 then $r[A] := $r[B]
*  Parameters: Register indices
*  Returns: none
*  Effects: Checked runtime error is UM is null or
*           if A, B, or C are bigger than 8
*/
inline void conditional_move(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        if (UM->registers[C] != 0)
                UM->registers[A] = UM->registers[B];
}

/* Name: segmented_load
*  Purpose: $r[A] := $m[$r[B]][$r[C]]
*  Parameters: B stores the segment ID, C stores the offset, A stores result
*  Returns: none
*  Effects: none
*/
inline void segmented_load(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        uint32_t segment_ID = UM->registers[B];
        uint32_t offset = UM->registers[C];

        UM->registers[A] = UM->segments[segment_ID][offset + 1];
}

/* Name: segmented_store
*  Purpose:  Segmented Store $m[$r[A]][$r[B]] := $r[C]
*  Parameters: A holds the segment ID, B stores the offset, C stores value to
*  store in UM
*  Returns: none
*  Effects: none
*/
inline void segmented_store(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        uint32_t segment_ID = UM->registers[A];
        uint32_t offset = UM->registers[B];

        UM->segments[segment_ID][offset + 1] = UM->registers[C];
}
/* Name: addition
*  Purpose: Add registers to update one
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: none
*/
inline void addition(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        UM->registers[A] = (UM->registers[B] + UM->registers[C]) % mod_limit;
}

/* Name: multiplication
*  Purpose: multiply registers to update one 
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: none
*/
inline void multiplication(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        UM->registers[A] = (UM->registers[B] * UM->registers[C]) % mod_limit;
}

/* Name: division
*  Purpose: divide register to update one
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: Checked runtime error for divide by 0
*/
inline void division(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        UM->registers[A] = (UM->registers[B] / UM->registers[C]) % mod_limit;
}

/* Name: bitwise_nand
*  Purpose: get the not of register B and C
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: updates register A
*/
inline void bitwise_nand(universal_machine UM, UM_Reg A, UM_Reg B, UM_Reg C)
{
        UM->registers[A] = ~(UM->registers[B] & UM->registers[C]);
}

/* Name: map_segment
*  Purpose: new segment is created
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: new segment is created
*/
inline void map(universal_machine UM, UM_Reg B, UM_Reg C)
{
        UM->registers[B] = map_segment(UM, UM->registers[C]);
}

/* Name: unmap_segment
*  Purpose: segment $m[$r[c]] is unmapped
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: segment $m[$r[c]] is unmapped
*/
inline void unmap(universal_machine UM, UM_Reg C)
{
        unmap_segment(UM, UM->registers[C]);
}

/* Name: unmap_segment
*  Purpose: segment $m[$r[c]] is unmapped
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: Checked runtime error if value from register c
*           is more than 255
*/
inline void output(universal_machine UM, UM_Reg C)
{
        putchar(UM->registers[C]);
}

/* Name: input
*  Purpose: Universal machine awaits input from I/O devise
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: instruction depend on I/O
*           Checked runtime error if value is
*.          out of range (has to be between 0 and 255)
*/
inline void input(universal_machine UM, UM_Reg C)
{
        int int_value = getchar();

        if (int_value == EOF)
                UM->registers[C] = ~0;
        else 
                UM->registers[C] = int_value;
}

/* Name: load_program
*  Purpose: segment $m[$r[B]] is duplicated and replaces $m[0]
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Note: Program counter is redirected in another module 
*        Checked runtime if target or duplicates are NULL 
*/
inline void load_program(universal_machine UM, UM_Reg B)
{
        uint32_t reg_B_value = UM->registers[B];

        /* Not allowed to load segment zero into segment zero */
        if (reg_B_value != 0) {
                uint32_t *target_segment = UM->segments[reg_B_value];

                uint32_t num_instructions = target_segment[0];

                uint32_t *deep_copy = malloc((num_instructions + 1) * sizeof(uint32_t));
                assert(deep_copy);

                uint32_t true_size = num_instructions + 1;
                for (size_t i = 0; i < true_size; i++)
                        deep_copy[i] = target_segment[i];

                free(UM->segments[0]);

                UM->segments[0] = deep_copy;
        }       
}

/* Name: load_value
*  Purpose: set $r[A] to value
*  Parameters: UM, A, red_B, C
*  Returns: none
*  Effects: changes register A
*/
inline void load_value(universal_machine UM, UM_Reg A, uint32_t value)
{
        UM->registers[A] = value;
}

/*************************************************************************
                        End Instruction Set Module 
*************************************************************************/

/*************************************************************************
                        Start Program Main Module 
*************************************************************************/

static inline uint32_t get_reg_C(uint64_t word)
{
        return word & 7;
}

// static inline uint32_t get_reg_B(uint64_t word)
// {
//         return word & 72;
// }

// static inline uint32_t get_reg_A(uint64_t word)
// {
//         return word & 448;
// }

static inline uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
        return (word << (64 - (lsb + width))) >> (64 - width); 
}

static inline uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb,
                      uint64_t value)
{
        return (word ^= Bitpack_getu(word, width, lsb) << lsb) | (value << lsb);
}

universal_machine read_program_file(FILE *fp)
{
        assert(fp != NULL);

        uint32_t *segment_zero = malloc(100 * sizeof(uint32_t));
        uint32_t segment_size = 100;
        uint32_t num_elems = 0;

        uint32_t word = 0;

        int byte = fgetc(fp);

        while (byte != EOF) {
                word = 0;

                for (int i = 3; i >= 0; i--) {
                        word = Bitpack_newu(word, 8, i * 8, byte);
                        byte = fgetc(fp);
                }

                /* If num elems + size elem is equal to total malloc'd size */
                if (num_elems + 1 == segment_size) {
                        uint32_t bigger_size = segment_size * 2;
                        segment_zero = realloc(segment_zero, bigger_size * sizeof(uint32_t));
                        segment_zero[num_elems + 1] = word;
                        segment_size = bigger_size;
                }
                else
                        segment_zero[num_elems + 1] = word;

                num_elems++;
        }

        segment_zero[0] = num_elems;

        universal_machine UM = new_UM(segment_zero);

        return UM;
}

/* Name: run_program
 * Purpose: Command loop for each machine cycle 
 * Parameters: Pointer to instance of universal machine
 * Returns: Void
 * Effects: Checked runtime error if program counter is out of bounds, invalid
 * OP_CODE, and if segment zero was unavailable 
 */
void run_program(universal_machine UM)
{
        assert(UM != NULL);

        while (true) {
                uint32_t *segment_zero = UM->segments[0];

                UM_instruction word = segment_zero[UM->program_counter + 1];

                int OP_CODE = word >> 28;

                /* Shift before anding ? */
                if (OP_CODE == LOAD_VALUE) {
                        load_value(UM, (word >> 25) & 7, (word << 7) >> 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == SEGMENTED_LOAD) {
                        segmented_load(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == SEGMENTED_STORE) {
                        segmented_store(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == BITWISE_NAND) {
                        bitwise_nand(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == ADDITION) {
                        addition(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == LOAD_PROGRAM) {
                        load_program(UM, (word >> 3) & 7);
                        UM->program_counter = UM->registers[word & 7];
                }
                else if (OP_CODE == CONDITIONAL_MOVE) {
                        conditional_move(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == MAP_SEGMENT) {
                        map(UM, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == UNMAP_SEGMENT) {
                        unmap(UM, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == DIVISION) {
                        division(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == MULTIPLICATION) {
                        multiplication(UM, (word >> 6) & 7, (word >> 3) & 7, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == OUTPUT) {
                        output(UM, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == INPUT) {
                        input(UM, word & 7);
                        UM->program_counter++;
                }
                else if (OP_CODE == HALT)
                        return;
        }
}

int main(int argc, char *argv[])
{
        if (argc != 2) exit(EXIT_FAILURE);

        FILE *fp = fopen(argv[1], "rb");
        assert(fp);

        struct stat file_info;
        stat(argv[1], &file_info);

        int arraysize = (int) (file_info.st_size / 4);

        uint32_t *segment_zero = malloc((arraysize + 1) * sizeof(uint32_t));

        uint32_t word = 0;

        int num_elems = 0;

        int byte = fgetc(fp);

        while (byte != EOF) {
                word = 0;

                for (int i = 3; i >= 0; i--) {
                        word = Bitpack_newu(word, 8, i * 8, byte);
                        byte = fgetc(fp);
                }

                segment_zero[num_elems + 1] = word;

                num_elems++;
        }

        segment_zero[0] = num_elems;

        universal_machine UM = new_UM(segment_zero);

        run_program(UM);

        free_UM(&UM);

        fclose(fp);

        return 0;
}

/*************************************************************************
                        End Program Main Module 
*************************************************************************/