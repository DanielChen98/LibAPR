//
// Created by cheesema on 14/03/17.
//

#include <algorithm>
#include <iostream>

#include "Example_reconstruct_image.h"
#include "src/io/TiffUtils.hpp"


bool command_option_exists(char **begin, char **end, const std::string &option)
{
    return std::find(begin, end, option) != end;
}

char* get_command_option(char **begin, char **end, const std::string &option)
{
    char ** itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
    {
        return *itr;
    }
    return 0;
}

cmdLineOptions read_command_line_options(int argc, char **argv){

    cmdLineOptions result;

    if(argc == 1) {
        std::cerr << "Usage: \"Example_recon_img -i input_apr_file -d directory [-o outputfile]\"" << std::endl;
        exit(1);
    }

    if(command_option_exists(argv, argv + argc, "-i"))
    {
        result.input = std::string(get_command_option(argv, argv + argc, "-i"));
    } else {
        std::cout << "Input file required" << std::endl;
        exit(2);
    }

    if(command_option_exists(argv, argv + argc, "-d"))
    {
        result.directory = std::string(get_command_option(argv, argv + argc, "-d"));
    }

    if(command_option_exists(argv, argv + argc, "-o"))
    {
        result.output = std::string(get_command_option(argv, argv + argc, "-o"));
    }

    return result;

}

int main(int argc, char **argv) {

    // INPUT PARSING

    cmdLineOptions options = read_command_line_options(argc, argv);

    // Filename
    std::string file_name = options.directory + options.input;

    // Read the apr file into the part cell structure
    Part_timer timer;

    timer.verbose_flag = true;

    // APR datastructure
    APR<uint16_t> apr;

    //read file
    apr.read_apr(file_name);

    apr.name = options.output;

    //create mesh data structure for reconstruction
    MeshData<uint16_t> recon_pc;

    timer.start_timer("pc interp");
    //perform piece-wise constant interpolation
    apr.interp_img(recon_pc,apr.particles_intensities);

    timer.stop_timer();
    float elapsed_seconds = timer.t2 - timer.t1;

    std::cout << "PC recon " << (recon_pc.x_num*recon_pc.y_num*recon_pc.z_num)/(elapsed_seconds*1000000.0) << " million pixels per second"  <<  std::endl;

    std::string output_path = options.directory + apr.name + "_pc.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, recon_pc);

    //////////////////////////
    ///
    /// Create a particle dataset with the particle type and pc construct it
    ///
    ////////////////////////////

    //initialization of the iteration structures
    APRIterator<uint16_t> apr_iterator(apr); //this is required for parallel access
    uint64_t particle_number;

    //create particle dataset
    ExtraParticleData<uint16_t> type(apr);
    ExtraParticleData<uint16_t> level(apr);

    ExtraParticleData<uint16_t> x(apr);
    ExtraParticleData<uint16_t> y(apr);
    ExtraParticleData<uint16_t> z(apr);


    timer.start_timer("APR parallel iterator loop");

#ifdef HAVE_OPENMP
	#pragma omp parallel for schedule(static) private(particle_number) firstprivate(apr_iterator)
#endif
    for (particle_number = 0; particle_number < apr_iterator.total_number_particles(); ++particle_number) {
        //needed step for any parallel loop (update to the next part)
        apr_iterator.set_iterator_to_particle_by_number(particle_number);

        type[apr_iterator] = apr_iterator.type();
        level[apr_iterator] = apr_iterator.level();

        x[apr_iterator] = apr_iterator.x();
        y[apr_iterator] = apr_iterator.y();
        z[apr_iterator] = apr_iterator.z();
    }

    timer.stop_timer();

    MeshData<uint16_t> type_recon;

    apr.interp_img(type_recon,type);

    output_path = options.directory + apr.name + "_type.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, type_recon);

    //pc interp
    apr.interp_img(type_recon,level);

    output_path = options.directory + apr.name + "_level.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, type_recon);


    //pc interp
    apr.interp_img(type_recon,x);

    output_path = options.directory + apr.name + "_x.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, type_recon);

    //pc interp
    apr.interp_img(type_recon,y);

    output_path = options.directory + apr.name + "_y.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, type_recon);

    //pc interp
    apr.interp_img(type_recon,z);

    output_path = options.directory + apr.name + "_z.tif";

    //write output as tiff
    TiffUtils::saveMeshAsTiff(output_path, type_recon);

    //smooth reconstruction - requires float
    MeshData<float> recon_smooth;

    std::vector<float> scale_d = {2,2,2};

    timer.start_timer("smooth reconstrution");

    apr.interp_parts_smooth(recon_smooth,apr.particles_intensities,scale_d);

    timer.stop_timer();
    elapsed_seconds = timer.t2 - timer.t1;

    std::cout << "Smooth recon " << (recon_smooth.x_num*recon_smooth.y_num*recon_smooth.z_num)/(elapsed_seconds*1000000.0) << " million pixels per second"  <<  std::endl;

    output_path = options.directory + apr.name + "_smooth.tif";

    //write to tiff casting to unsigned 16 bit integer
    TiffUtils::saveMeshAsTiffUint16(output_path, recon_smooth);
}