
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

typedef unsigned UM_Reg;
typedef uint32_t UM_instruction;

#define mod_limit 4294967296;

static inline uint64_t Bitpack_getu(uint64_t word, unsigned width, unsigned lsb)
{
        return (word << (64 - (lsb + width))) >> (64 - width); 
}

static inline uint64_t Bitpack_newu(uint64_t word, unsigned width, unsigned lsb,
                      uint64_t value)
{
        return (word ^= Bitpack_getu(word, width, lsb) << lsb) | (value << lsb);
}

int main(int argc, char *argv[])
{
        if (argc != 2) exit(EXIT_FAILURE);

        /**** LOAD PROGRAM ****/
        FILE *fp = fopen(argv[1], "rb");
        assert(fp);

        struct stat file_info;
        stat(argv[1], &file_info);

        int num_4byte_words = file_info.st_size / 4;

        uint32_t *segment_zero = malloc((num_4byte_words + 1) * sizeof(uint32_t));
        assert(segment_zero);

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

        fclose(fp);

        segment_zero[0] = num_elems;
        /**** END LOAD PROGRAM ****/


        /* Initialize Data Members */
        uint32_t registers[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        uint32_t program_counter = 0;

        uint32_t *unmapped_IDs = malloc(1 * sizeof(uint32_t));
        assert(unmapped_IDs);
        uint32_t num_IDs = 0;
        uint32_t ID_arr_size = 1;

        uint32_t **segments = malloc(1 * sizeof(uint32_t *));
        assert(segments);
        uint32_t segment_arr_size = 1;
        uint32_t total_seg_space = 1;

        segments[0] = segment_zero;
        /* End Constructor */

        /* Start Run Program */
        uint32_t *segments_zero;
        (void) segments_zero;

        int OP_CODE;
        (void) OP_CODE;

        /* Start Run Program */
        while (true) {
                segment_zero = segments[0];

                UM_instruction word = segment_zero[program_counter + 1];

                OP_CODE = word >> 28;

                /* Shift before anding ? */
                if (OP_CODE == LOAD_VALUE) {
                        registers[(word >> 25) & 7] = (word << 7) >> 7;
                        program_counter++;
                }
                else if (OP_CODE == SEGMENTED_LOAD) {
                        registers[(word >> 6) & 7] = segments[registers[(word >> 3) & 7]][registers[word & 7] + 1];
                        program_counter++;
                }
                else if (OP_CODE == SEGMENTED_STORE) {
                        segments[registers[(word >> 6) & 7]][registers[(word >> 3) & 7] + 1] = registers[word & 7];
                        program_counter++;
                }
                else if (OP_CODE == BITWISE_NAND) {
                        registers[(word >> 6) & 7] = ~(registers[(word >> 3) & 7] & registers[word & 7]);
                        program_counter++;
                }
                else if (OP_CODE == ADDITION) {
                        registers[(word >> 6) & 7] = (registers[(word >> 3) & 7] + registers[word & 7]) % mod_limit;
                        program_counter++;
                }
                else if (OP_CODE == LOAD_PROGRAM) {
                        uint32_t reg_B_value = registers[(word >> 3) & 7];

                        /* Not allowed to load segment zero into segment zero */
                        if (reg_B_value != 0) {
                                uint32_t *target_segment = segments[reg_B_value];

                                uint32_t num_instructions = target_segment[0];

                                uint32_t *deep_copy = malloc((num_instructions + 1) * sizeof(uint32_t));
                                assert(deep_copy);

                                uint32_t true_size = num_instructions + 1;
                                for (size_t i = 0; i < true_size; i++)
                                        deep_copy[i] = target_segment[i];

                                free(segments[0]);

                                segments[0] = deep_copy;
                        }       

                        program_counter = registers[word & 7];
                }
                else if (OP_CODE == CONDITIONAL_MOVE) {
                        if (registers[word & 7] != 0)
                                registers[(word >> 6) & 7] = registers[(word >> 3) & 7];

                        program_counter++;
                }
                else if (OP_CODE == MAP_SEGMENT) {
                        uint32_t *new_segment = calloc(registers[word & 7] + 1, sizeof(uint32_t));
                        assert(new_segment);

                        /* First elem stores the number of words */
                        new_segment[0] = registers[word & 7];
                        
                        /* Case 1: If there are no unmapped IDs */
                        if (num_IDs == 0) {
                                /* Check whether realloc is necessary for segments spine */
                                if (total_seg_space == segment_arr_size) {
                                        uint32_t bigger_arr_size = segment_arr_size * 2;
                                        segments = realloc(segments, bigger_arr_size * sizeof(uint32_t *));
                                        assert(segments);
                                        segment_arr_size = bigger_arr_size;
                                }

                                segments[total_seg_space] = new_segment;

                                total_seg_space++;

                                registers[(word >> 3) & 7] = total_seg_space - 1;
                        }
                        /* Case 2: There are unmapped IDs available for use */
                        else {
                                /* Back most element of array / top of stack */
                                uint32_t available_ID = unmapped_IDs[num_IDs - 1];
                                num_IDs--;

                                /* Free data that has been there */
                                uint32_t *to_unmap = segments[available_ID];
                                free(to_unmap);

                                segments[available_ID] = new_segment;

                                registers[(word >> 3) & 7] = available_ID;
                        }

                        program_counter++;
                }
                else if (OP_CODE == UNMAP_SEGMENT) {
                        /* Add the new ID to the ID C-array */
                        if (num_IDs == ID_arr_size) {
                                uint32_t bigger_arr_size = ID_arr_size * 2;
                                unmapped_IDs = realloc(unmapped_IDs, bigger_arr_size * sizeof(uint32_t));
                                assert(unmapped_IDs);
                                ID_arr_size = bigger_arr_size;
                        }

                        /* Push the newly available ID to the top of the stack */
                        unmapped_IDs[num_IDs] = registers[word & 7];

                        /* Update number of IDs and number of segments */
                        num_IDs++;

                        program_counter++;
                }
                else if (OP_CODE == DIVISION) {
                        registers[(word >> 6) & 7] = (registers[(word >> 3) & 7] / registers[word & 7]);
                        program_counter++;
                }
                else if (OP_CODE == MULTIPLICATION) {
                        registers[(word >> 6) & 7] = (registers[(word >> 3) & 7] * registers[word & 7]) % mod_limit;
                        program_counter++;
                }
                else if (OP_CODE == OUTPUT) {
                        putchar(registers[word & 7]);
                        program_counter++;
                }
                else if (OP_CODE == INPUT) {
                        int int_value = getchar();

                        if (int_value == EOF)
                                registers[word & 7] = ~0;
                        else
                                registers[word & 7] = int_value;

                        program_counter++;
                }
                else if (OP_CODE == HALT)
                        break;
        }

        /* Free the data */
        for (size_t i = 0; i < total_seg_space; i++)
                free(segments[i]);
        
        free(segments);
        free(unmapped_IDs);

        return 0;
}