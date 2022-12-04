
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
        uint32_t num_segments;
        uint32_t segment_arr_size;

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
        UM->num_segments = 1;
        UM->segment_arr_size = 1;

        /* Store pointer to first word instruction in segment zero */
        UM->segments[0] = program_instructions;

        return UM;
}

void free_UM(universal_machine *UM)
{
        /* Free malloc'd 32-bit instruction segments */

        uint32_t **spine = (*UM)->segments;

                
        for (size_t i = 0; i < (*UM)->num_segments; i++)
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
                if (UM->num_segments == UM->segment_arr_size) {
                        uint32_t bigger_arr_size = UM->segment_arr_size * 2;
                        UM->segments = realloc(UM->segments, bigger_arr_size * sizeof(uint32_t *));
                        assert(UM->segments);
                        UM->segment_arr_size = bigger_arr_size;
                }

                UM->segments[UM->num_segments] = new_segment;

                UM->num_segments++;

                return UM->num_segments - 1;
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

                UM->num_segments++;

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
        UM->num_segments--;
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

Except_T Bitpack_Overflow = { "Overflow packing bits" };

static inline uint64_t shl(uint64_t word, unsigned bits)
{
        assert(bits <= 64);
        if (bits == 64)
                return 0;
        else
                return word << bits;
}

static inline uint64_t shr(uint64_t word, unsigned bits)
{
        assert(bits <= 64);
        if (bits == 64)
                return 0;
        else
                return word >> bits;
}

static inline bool Bitpack_fitsu(uint64_t n, unsigned width)
{
        assert(width <= 64);
        /* thanks to Jai Karve and John Bryan  */
        /* clever shortcut instead of 2 shifts */
        return shr(n, width) == 0; 
}

static inline uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb,
                      uint64_t value)
{
        assert(width <= 64);
        unsigned hi = lsb + width; /* one beyond the most significant bit */
        assert(hi <= 64);
        if (!Bitpack_fitsu(value, width)) {
                printf("Value : %lu\n", value);
                printf("width : %u\n", width);

                RAISE(Bitpack_Overflow);
        }
        return shl(shr(word, hi), hi)                 /* high part */
                | shr(shl(word, 64 - lsb), 64 - lsb)  /* low part  */
                | (value << lsb);                     /* new part  */
}

static inline uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
        assert(width <= 64);
        unsigned hi = lsb + width; /* one beyond the most significant bit */
        assert(hi <= 64);
        /* different type of right shift */
        return shr(shl(word, 64 - hi),
                   64 - width); 
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

        fprintf(stderr, "POOPY BUTT\n");

        while (true) {
                uint32_t *segment_zero = UM->segments[0];

                UM_instruction word = segment_zero[UM->program_counter + 1];

                int OP_CODE = Bitpack_getu(word, 4, 28);
               
                UM_Reg A, B, C;

                // printf("PROGRAM COUNTER: %d\n", UM->program_counter);

                // switch (OP_CODE) {
                //         case 0:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 conditional_move(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 1:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 segmented_load(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 2:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 segmented_store(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 3:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 addition(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 4:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 multiplication(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 5:
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 division(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 6: 
                //                 A = Bitpack_getu(word, 3, 6);
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 bitwise_nand(UM, A, B, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 7:
                //                 return;
                //         case 8:
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 map(UM, B, C);
                //                 UM->program_counter++;
                //                 break;

                //         case 9:
                //                 C = Bitpack_getu(word, 3, 0);
                //                 unmap(UM, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 10:
                //                 C = Bitpack_getu(word, 3, 0);
                //                 output(UM, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 11:
                //                 C = Bitpack_getu(word, 3, 0);
                //                 input(UM, C);
                //                 UM->program_counter++;
                //                 break;
                //         case 12:
                //                 B = Bitpack_getu(word, 3, 3);
                //                 C = Bitpack_getu(word, 3, 0);
                //                 load_program(UM, B);
                //                 UM->program_counter = UM->registers[C];
                //                 break;
                                
                // }




                /* Halt Command, exit function to free data */
                if (OP_CODE == 7) {
                        return;
                }
                /* Special Load Value Command */
                else if (OP_CODE == 13) {
                        A = Bitpack_getu(word, 3, 25);
                        int load_val = Bitpack_getu(word, 25, 0);
                        
                        load_value(UM, A, load_val);
                }
                /* Other 12 instructions */
                else {
                        A = Bitpack_getu(word, 3, 6);
                        B = Bitpack_getu(word, 3, 3);
                        C = Bitpack_getu(word, 3, 0);

                        switch (OP_CODE) {
                                case 0:
                                        conditional_move(UM, A, B, C);
                                        break;
                                case 1:
                                        segmented_load(UM, A, B, C);
                                        break;
                                case 2:
                                        segmented_store(UM, A, B, C);
                                        break;
                                case 3:
                                        addition(UM, A, B, C);
                                        break;
                                case 4:
                                        multiplication(UM, A, B, C);
                                        break;
                                case 5:
                                        division(UM,  A, B, C);
                                        break;
                                case 6:
                                        bitwise_nand(UM, A, B, C);
                                        break;
                                case 8:
                                        map(UM, B, C);
                                        break;
                                case 9:
                                        unmap(UM, C);
                                        break;
                                case 10:
                                        output(UM, C);
                                        break;
                                case 11:
                                        input(UM, C);
                                        break;
                                case 12:
                                        load_program(UM, B);
                                        break;
                        }
                }
        
                if (OP_CODE == 12)
                        UM->program_counter = UM->registers[C];
                else
                        UM->program_counter++;
        }
}

int main(int argc, char *argv[])
{
        assert(argc == 2);

        FILE *fp = fopen(argv[1], "rb");
        
        universal_machine UM = read_program_file(fp);

        run_program(UM);

        free_UM(&UM);

        fclose(fp);

        return 0;
}

/*************************************************************************
                        End Program Main Module 
*************************************************************************/