// Released under MIT License.
// Copyright (c) 2022 Ladislav Bartos

#include <stdio.h>
#include <unistd.h>
#include <groan.h>

const char VERSION[] = "v2022/06/25";

// frequency of printing during the calculation
const int PROGRESS_FREQ = 10000;
// inverse size of a grid tile for membrane thickness calculation
const int GRID_TILE = 10;

/*
 * Parses command line arguments.
 * Returns zero, if parsing has been successful. Else returns non-zero.
 */
int get_arguments(
        int argc, 
        char **argv,
        char **gro_file,
        char **xtc_file,
        char **ndx_file,
        char **output_file,
        char **lipids,
        char **phosphates,
        float *array_dimx,
        float *array_dimy,
        int   *nan_limit
        ) 
{
    int gro_specified = 0, xtc_specified = 0;

    int opt = 0;
    while((opt = getopt(argc, argv, "c:f:n:o:l:p:x:y:a:h")) != -1) {
        switch (opt) {
        // help
        case 'h':
            return 1;
        // gro file to read
        case 'c':
            *gro_file = optarg;
            gro_specified = 1;
            break;
        // xtc file to read
        case 'f':
            *xtc_file = optarg;
            xtc_specified = 1;
            break;
        // ndx file to read
        case 'n':
            *ndx_file = optarg;
            break;
        // output file name
        case 'o':
            *output_file = optarg;
            break;
        // specification of the lipids
        case 'l':
            *lipids = optarg;
            break;
        // specification of the phosphates
        case 'p':
            *phosphates = optarg;
            break;
        // specification of array dimensions (x axis)
        case 'x':
            if (sscanf(optarg, "%f-%f", &array_dimx[0], &array_dimx[1]) != 2 && 
                sscanf(optarg, "%f - %f", &array_dimx[0], &array_dimx[1]) != 2 &&
                sscanf(optarg, "%f %f", &array_dimx[0], &array_dimx[1]) != 2) {
                fprintf(stderr, "Could not understand grid x-dimension specifier.\n");
                return 1;
            }
            break;
        // specification of array dimensions (y axis)
        case 'y':
            if (sscanf(optarg, "%f-%f", &array_dimy[0], &array_dimy[1]) != 2 && 
                sscanf(optarg, "%f - %f", &array_dimy[0], &array_dimy[1]) != 2 &&
                sscanf(optarg, "%f %f", &array_dimy[0], &array_dimy[1]) != 2) {
                fprintf(stderr, "Could not understand grid y-dimension specifier.\n");
                return 1;
            }
            break;
        // specification of the nan limit
        case 'a':
            sscanf(optarg, "%d", nan_limit);
            break;
        default:
            //fprintf(stderr, "Unknown command line option: %c.\n", opt);
            return 1;
        }
    }

    if (!gro_specified || !xtc_specified) {
        fprintf(stderr, "Gro file and xtc file must always be supplied.\n");
        return 1;
    }
    return 0;
}

void print_usage(const char *program_name)
{
    printf("Usage: %s -c GRO_FILE -f XTC_FILE [OPTION]...\n", program_name);
    printf("\nOPTIONS\n");
    printf("-h               print this message and exit\n");
    printf("-c STRING        gro file to read\n");
    printf("-f STRING        xtc file to read\n");
    printf("-n STRING        ndx file to read (optional, default: index.ndx)\n");
    printf("-o STRING        output file name (default: membrane_thickness.dat)\n");
    printf("-l STRING        specification of membrane lipids (default: Membrane)\n");
    printf("-p STRING        specification of lipid phosphates (default: name PO4)\n");
    printf("-x FLOAT-FLOAT   grid dimensions in x axis (default: box size from gro file)\n");
    printf("-y FLOAT-FLOAT   grid dimensions in y axis (default: box size from gro file)\n");
    printf("-a INTEGER       NAN limit: how many phosphates must be detected in a grid tile\n");
    printf("                 to calculate membrane thickness for this tile (default: 30)\n");
    printf("\n");
}

/*
 * Prints parameters that the program will use for the calculation.
 */
void print_arguments(
        FILE *stream,
        const char *gro_file, 
        const char *xtc_file,
        const char *ndx_file,
        const char *output_file,
        const char *lipids,
        const char *phosphates,
        const float *array_dimx,
        const float *array_dimy,
        const int nan_limit)
{
    fprintf(stream, "Parameters for Membrane Thickness calculation:\n");
    fprintf(stream, ">>> gro file:         %s\n", gro_file);
    fprintf(stream, ">>> xtc file:         %s\n", xtc_file);
    fprintf(stream, ">>> ndx file:         %s\n", ndx_file);
    fprintf(stream, ">>> output file:      %s\n", output_file);
    fprintf(stream, ">>> lipids:           %s\n", lipids);
    fprintf(stream, ">>> phosphates:       %s\n", phosphates);
    fprintf(stream, ">>> grid dimensions:  x: %.1f - %.1f nm, y: %.1f - %.1f nm\n", array_dimx[0], array_dimx[1], array_dimy[0], array_dimy[1]);
    fprintf(stream, ">>> NAN limit:        %d\n\n", nan_limit);
}

/* 
 * Converts index of an array to coordinate.
 */
static inline float index2coor(int x, float minx)
{
    return (float) x / GRID_TILE + minx;
}

/*
 * Converts coordinate to an index array.
 */
static inline size_t coor2index(float x, float minx)
{
    return (size_t) roundf((x - minx) * GRID_TILE);
}

int main(int argc, char **argv)
{
    printf("\n");
    // get command line arguments
    char *gro_file = NULL;
    char *xtc_file = NULL;
    char *ndx_file = "index.ndx";
    char *output_file = "membrane_thickness.dat";
    char *lipids   = "Membrane";
    char *phosphates = "name PO4";
    float array_dimx[2] = {0.};
    float array_dimy[2] = {0.};
    int nan_limit = 30;
    if (get_arguments(argc, argv, &gro_file, &xtc_file, &ndx_file, &output_file, &lipids, &phosphates, array_dimx, array_dimy, &nan_limit) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    // check that the nan limit is > 0
    if (nan_limit <= 0) {
        fprintf(stderr, "NAN limit must be higher than 0.\n");
        return 1;
    }

    // we open the output file quite early to check that it can actually be opened
    // we do not want to calculate everything and then find out that the output file is unreachable
    FILE *output = fopen(output_file, "w");
    if (output == NULL) {
        fprintf(stderr, "Output file could not be opened.\n");
        return 1;
    }

    // read gro file
    system_t *system = load_gro(gro_file);
    if (system == NULL) return 1;

    // if array dimensions were not set, get them from gro file
    // (yes, i know... direct comparing of float to 0, not a good idea, blah blah... but here it should work)
    if (array_dimx[0] == 0 && array_dimx[1] == 0) {
        array_dimx[1] = system->box[0];  
    }
    if (array_dimy[0] == 0 && array_dimy[1] == 0) {
        array_dimy[1] = system->box[1];
    }

    // check that the array dimensions don't have nonsensical values
    if (array_dimx[0] >= array_dimx[1] || array_dimy[0] >= array_dimy[1]) {
        fprintf(stderr, "Nonsensical array dimensions.\n");
        free(system);
        return 1;
    }

    print_arguments(stdout, gro_file, xtc_file, ndx_file, output_file, lipids, phosphates, array_dimx, array_dimy, nan_limit);

    // open xtc file for reading
    XDRFILE *xtc = xdrfile_open(xtc_file, "r");
    if (xtc == NULL) {
        fprintf(stderr, "File %s could not be read as an xtc file.\n", xtc_file);
        free(system);
        return 1;
    }

    // set all velocities of all particles to zero (xtc does not contain velocities)
    reset_velocities(system);

    // check that the gro file and the xtc file match each other
    if (!validate_xtc(xtc_file, (int) system->n_atoms)) {
        fprintf(stderr, "Number of atoms in %s does not match %s.\n", xtc_file, gro_file);
        xdrfile_close(xtc);
        free(system);
        return 1;
    }

    // read ndx file
    dict_t *ndx_groups = read_ndx(ndx_file, system);

    // select all atoms
    atom_selection_t *all = select_system(system);

    // select membrane
    atom_selection_t *membrane_atoms = smart_select(all, lipids, ndx_groups);
    if (membrane_atoms == NULL || membrane_atoms->n_atoms == 0) {
        fprintf(stderr, "No lipid atoms detected.\n");
        dict_destroy(ndx_groups);
        xdrfile_close(xtc);
        free(all);
        free(membrane_atoms);
        free(system);
        return 1;
    }

    // select phosphates
    atom_selection_t *phosphate_atoms = smart_select(all, phosphates, ndx_groups);
    if (phosphate_atoms == NULL || phosphate_atoms->n_atoms == 0) {
        fprintf(stderr, "No phosphate atoms detected.\n");
        dict_destroy(ndx_groups);
        xdrfile_close(xtc);
        free(all);
        free(membrane_atoms);
        free(phosphate_atoms);
        free(system);
        return 1;
    }

    // prepare arrays
    size_t n_rows = (size_t) roundf( (array_dimy[1] - array_dimy[0]) * GRID_TILE ) + 1;
    size_t n_cols = (size_t) roundf( (array_dimx[1] - array_dimx[0]) * GRID_TILE ) + 1;
    size_t n_tiles = n_rows * n_cols;

    float *upper_leaflet        = calloc(n_tiles, sizeof(float));
    int   *upper_leaflet_counts = calloc(n_tiles, sizeof(int));
    float *lower_leaflet        = calloc(n_tiles, sizeof(float));
    int   *lower_leaflet_counts = calloc(n_tiles, sizeof(int));

    if (upper_leaflet == NULL || upper_leaflet_counts == NULL || lower_leaflet == NULL || lower_leaflet_counts == NULL) {
        fprintf(stderr, "Could not allocate memory (grid too large?)\n");
        dict_destroy(ndx_groups);
        xdrfile_close(xtc);
        fclose(output);
        free(all);
        free(membrane_atoms);
        free(phosphate_atoms);
        free(system);
        return 1;
    }

    while (read_xtc_step(xtc, system) == 0) {

        // print info about the progress of reading
        if ((int) system->time % PROGRESS_FREQ == 0) {
            printf("Step: %d. Time: %.0f ps\r", system->step, system->time);
            fflush(stdout);
        }

        // get membrane center
        vec_t center_mem = {0};
        center_of_geometry(membrane_atoms, center_mem, system->box);

        // loop through phosphates, assign them to leaflets... 
        // ...and get their z positions relative to center_mem
        for (size_t i = 0; i < phosphate_atoms->n_atoms; ++i) {
            atom_t *atom = phosphate_atoms->atoms[i];
            float rel_pos_z = distance1D(atom->position, center_mem, z, system->box);

            // ignore atoms that are outside of the specified grid
            if (atom->position[0] < array_dimx[0] || atom->position[0] > array_dimx[1] ||
                atom->position[1] < array_dimy[0] || atom->position[1] > array_dimy[1]) {
                    continue;
                }
            
            // get index of the tile to which the atom should be assigned
            size_t x_index = coor2index(atom->position[0], array_dimx[0]);
            size_t y_index = coor2index(atom->position[1], array_dimy[0]);

            if (rel_pos_z > 0) {
                upper_leaflet[y_index * n_cols + x_index] += rel_pos_z;
                ++upper_leaflet_counts[y_index * n_cols + x_index];
            } else {
                lower_leaflet[y_index * n_cols + x_index] += rel_pos_z;
                ++lower_leaflet_counts[y_index * n_cols + x_index];
            }
        }

    }

    // write header for the output file
    fprintf(output, "# Generated with memthick (C Membrane Thickness Calculator) %s\n", VERSION);
    fprintf(output, "# Command line: ");
    for (int i = 0; i < argc; ++i) {
        fprintf(output, "%s ", argv[i]);
    }
    fprintf(output, "\n# See average membrane thickness at the end of this file.\n");
    fprintf(output, "@ xlabel x coordinate [nm]\n");
    fprintf(output, "@ ylabel y coordinate [nm]\n");
    fprintf(output, "@ zlabel membrane thickness [nm]\n");
    fprintf(output, "@ grid --\n");
    fprintf(output, "$ type colorbar\n");
    fprintf(output, "$ colormap rainbow\n");

    float av_thickness = 0;
    int n_samples = 0;

    // calculate final thickness and write it into the output file
    for (size_t y = 0; y < n_rows; ++y) {
        for (size_t x = 0; x < n_cols; ++x) {

            // check that we have enough data for this grid tile
            if (upper_leaflet_counts[y * n_cols + x] < nan_limit || lower_leaflet_counts[y * n_cols + x] < nan_limit ) {
                fprintf(output, "%f %f nan\n", index2coor(x, array_dimx[0]), index2coor(y, array_dimy[0]));
                continue;
            }

            float thickness = (upper_leaflet[y * n_cols + x] / upper_leaflet_counts[y * n_cols + x]) - 
                              (lower_leaflet[y * n_cols + x] / lower_leaflet_counts[y * n_cols + x]);
            
            av_thickness += thickness;
            ++n_samples;

            fprintf(output, "%f %f %.4f\n", index2coor(x, array_dimx[0]), index2coor(y, array_dimy[0]), thickness);
        }
    }

    av_thickness = av_thickness / n_samples;
    printf("\nAverage membrane thickness: %.4f nm\n", av_thickness);
    fprintf(output, "# Average membrane thickness: %.4f nm\n", av_thickness);

    dict_destroy(ndx_groups);
    xdrfile_close(xtc);
    fclose(output);
    free(all);
    free(membrane_atoms);
    free(phosphate_atoms);
    free(system);

    free(upper_leaflet);
    free(upper_leaflet_counts);
    free(lower_leaflet);
    free(lower_leaflet_counts);

    return 0;
}