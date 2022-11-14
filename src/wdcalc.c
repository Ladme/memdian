// Released under MIT License.
// Copyright (c) 2022 Ladislav Bartos

#include <unistd.h>
#include <groan.h>

// frequency of printing during the calculation
const int PROGRESS_FREQ = 10000;

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
        char **lipids,
        char **protein,
        char **water,
        float *radius,
        float *height
        ) 
{
    int gro_specified = 0;

    int opt = 0;
    while((opt = getopt(argc, argv, "c:f:l:n:r:e:p:w:h")) != -1) {
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
            break;
        // specification of the lipids (residue names)
        case 'l':
            *lipids = optarg;
            break;
        // ndx file to read
        case 'n':
            *ndx_file = optarg;
            break;
        // specification of the protein (atom names)
        case 'p':
            *protein = optarg;
            break;
        // specification of the water (single atom name)
        case 'w':
            *water = optarg;
            break;
        // radius for the water defect cylinder
        case 'r':
            *radius = atof(optarg);
            if (*radius <= 0) {
                fprintf(stderr, "Cylinder radius must be >0, not %f.\n", *radius);
                return 1;
            }
            break;
        // height of the water defect cylinder
        case 'e':
            *height = atof(optarg);
            if (*height <= 0) {
                fprintf(stderr, "Cylinder height must be >0, not %f.\n", *height);
                return 1;
            }
            break;
        default:
            //fprintf(stderr, "Unknown command line option: %c.\n", opt);
            return 1;
        }
    }

    if (!gro_specified) {
        fprintf(stderr, "Gro file must always be supplied.\n");
        return 1;
    }
    return 0;
}

void print_usage(const char *program_name)
{
    printf("Usage: %s -c GRO_FILE [OPTION]...\n", program_name);
    printf("\nOPTIONS\n");
    printf("-h          print this message and exit\n");
    printf("-c STRING   gro file to read\n");
    printf("-f STRING   xtc file to read (optional)\n");
    printf("-n STRING   ndx file to read (optional, default: index.ndx)\n");
    printf("-l STRING   specification of membrane lipids (default: Membrane) \n");
    printf("-p STRING   specification of protein; use \"no\" if there is no protein (default: Protein)\n");
    printf("-w STRING   specification of water (default: name W)\n");
    printf("-r FLOAT    radius of the water defect cylinder in nm (default: 2.5)\n");
    printf("-e FLOAT    height of the water defect cylinder in nm (default: 4.0)\n");
    printf("\n");
}

/*
 * Prints parameters that the program will use for the water defect calculation.
 */
void print_arguments(
        const char *gro_file, 
        const char *xtc_file,
        const char *ndx_file,
        const char *lipids,
        const char *protein,
        const char *water,
        const float radius,
        const float height)
{
    printf("Parameters for Water Defect calculation:\n");
    printf(">>> gro file:        %s\n", gro_file);
    if (xtc_file != NULL) printf(">>> xtc file:        %s\n", xtc_file);
    printf(">>> ndx file:        %s\n", ndx_file);
    printf(">>> lipids:          %s\n", lipids);
    if (strcmp(protein, "no")) printf(">>> protein:         %s\n", protein);
    else printf(">>> protein:         ---\n");
    printf(">>> water:           %s\n", water);
    printf(">>> cylinder radius: %f nm\n", radius);
    printf(">>> cylinder height: %f nm\n\n", height);
}

void calc_wd_frame(
        system_t *system,
        const atom_selection_t *membrane_atoms,
        const atom_selection_t *protein_atoms,
        const atom_selection_t *water_atoms,
        const float half_height,
        const float radius,
        size_t *upp_w_defect,
        size_t *low_w_defect)
{
    // get membrane center
    vec_t center_mem = {0};
    center_of_geometry(membrane_atoms, center_mem, system->box);

    // get protein center
    vec_t center_prot = {0};
    if (protein_atoms == NULL) {
        center_prot[0] = system->box[0] / 2;
        center_prot[1] = system->box[1] / 2;
    } else {
        center_of_geometry(protein_atoms, center_prot, system->box);
    }

    // calculate water defect
    for (size_t i = 0; i < water_atoms->n_atoms; ++i) {
        atom_t *atom = water_atoms->atoms[i];

        float dist = distance1D(atom->position, center_mem, z, system->box);
        if ((fabsf(dist) < half_height) && 
            (distance2D(atom->position, center_prot, xy, system->box) < radius)) {
                // upper leaflet water defect
                if (dist > 0) ++(*upp_w_defect);
                else ++(*low_w_defect);
        }
    }
}

int main(int argc, char **argv)
{
    printf("\n");
    // get command line arguments
    char *gro_file = NULL;
    char *xtc_file = NULL;
    char *lipids   = "Membrane";
    char *ndx_file = "index.ndx";
    char *protein  = "Protein";
    char *water    = "name W";
    float radius   = 2.5;
    float height   = 4.0;
    if (get_arguments(argc, argv, &gro_file, &xtc_file, &ndx_file, &lipids, &protein, &water, &radius, &height) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    // get half the height of the cylinder which will later be used for calculation
    float half_height = height / 2;

    print_arguments(gro_file, xtc_file, ndx_file, lipids, protein, water, radius, height);

    // read gro file
    system_t *system = load_gro(gro_file);
    if (system == NULL) return 1;

    // read ndx file
    dict_t *ndx_groups = read_ndx(ndx_file, system);

    // select all atoms
    atom_selection_t *all = select_system(system);

    // select membrane
    atom_selection_t *membrane_atoms = smart_select(all, lipids, ndx_groups);
    if (membrane_atoms == NULL || membrane_atoms->n_atoms == 0) {
        fprintf(stderr, "No lipid atoms detected.\n");
        dict_destroy(ndx_groups);
        free(all);
        free(membrane_atoms);
        free(system);
        return 1;
    }

    // select protein
    atom_selection_t *protein_atoms = NULL;
    if (strcmp(protein, "no")) {
        protein_atoms = smart_select(all, protein, ndx_groups);
        if (protein_atoms == NULL || protein_atoms->n_atoms == 0) {
            fprintf(stderr, "No protein atoms detected.\n");
            dict_destroy(ndx_groups);
            free(all);
            free(membrane_atoms);
            free(protein_atoms);
            free(system);
            return 1;
        }
    }

    // select water
    atom_selection_t *water_atoms = smart_select(all, water, ndx_groups);
    if (water_atoms == NULL || water_atoms->n_atoms == 0) {
        fprintf(stderr, "No water atoms detected.\n");
        dict_destroy(ndx_groups);
        free(all);
        free(membrane_atoms);
        free(protein_atoms);
        free(water_atoms);
        free(system);
        return 1;
    }

    
    size_t n_frames = 0;
    size_t upp_w_defect = 0;
    size_t low_w_defect = 0;
    int return_code = 0;

    // if there is no xtc file provided, analyze the gro file
    if (xtc_file == NULL) {
        ++n_frames;
        calc_wd_frame(system, membrane_atoms, protein_atoms, water_atoms, half_height, radius, &upp_w_defect, &low_w_defect);
    } else {
        // open xtc file for reading
        XDRFILE *xtc = xdrfile_open(xtc_file, "r");
        if (xtc == NULL) {
            fprintf(stderr, "File %s could not be read as an xtc file.\n", xtc_file);
            return_code = 1;
            goto function_end;
        }

        // check that the gro file and the xtc file match each other
        if (!validate_xtc(xtc_file, (int) system->n_atoms)) {
            fprintf(stderr, "Number of atoms in %s does not match %s.\n", xtc_file, gro_file);
            xdrfile_close(xtc);
            return_code = 1;
            goto function_end;
        }

        // read xtc
        while (read_xtc_step(xtc, system) == 0) {
            ++n_frames;
            // print info about the progress of reading
            if ((int) system->time % PROGRESS_FREQ == 0) {
                printf("Step: %d. Time: %.0f\r", system->step, system->time);
                fflush(stdout);
            }
            calc_wd_frame(system, membrane_atoms, protein_atoms, water_atoms, half_height, radius, &upp_w_defect, &low_w_defect);
        }

        xdrfile_close(xtc);
    }

    printf("\n\nAverage upper-leaflet water defect: % 8.4f\n", (float) (upp_w_defect) / n_frames);
    printf("Average lower-leaflet water defect: % 8.4f\n", (float) (low_w_defect) / n_frames);
    printf("Average water defect:               % 8.4f\n", (float) (upp_w_defect + low_w_defect) / n_frames);

    function_end:
    dict_destroy(ndx_groups);
    free(all);
    free(membrane_atoms);
    free(protein_atoms);
    free(water_atoms);
    free(system);

    return return_code;
}