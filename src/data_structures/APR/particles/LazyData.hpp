//
// Created by cheesema on 2019-06-15.
//

#ifndef LIBAPR_LAZYDATA_HPP
#define LIBAPR_LAZYDATA_HPP

#include <vector>
#include "GenData.hpp"
#include "io/APRFile.hpp"

template<typename DataType>
class LazyData: public GenData<DataType>  {

    uint64_t current_offset;

    std::vector<DataType> data;

    APRWriter::FileStructure* fileStructure;
    std::string parts_name;
    bool apr_or_tree;
    uint64_t parts_start;
    uint64_t parts_end;
    hid_t group_id;

public:

    void init_create_dataset(APRFile& parts_file,std::string name,bool apr_or_tree_){
        parts_name = name;
        apr_or_tree = apr_or_tree_;
        fileStructure = parts_file.get_fileStructure();
        fileStructure->create_time_point(0,apr_or_tree_,"t");

//        hid_t plist_id  = H5Pcreate(H5P_DATASET_CREATE);
//
//        // Dataset must be chunked for compression
//        const uint64_t max_size = 100000;
//        hsize_t cdims = (dims[0] < max_size) ? dims[0] : max_size;
//        rank = 1;
//        H5Pset_chunk(plist_id, rank, &cdims);
//
//        /////SET COMPRESSION TYPE /////
//        // But you can also taylor Blosc parameters to your needs
//        // 0 to 3 (inclusive) param slots are reserved.
//        const int numOfParams = 7;
//        unsigned int cd_values[numOfParams];
//        cd_values[4] = comp_level; // compression level
//        cd_values[5] = shuffle;    // 0: shuffle not active, 1: shuffle active
//        cd_values[6] = comp_type;  // the actual compressor to use
//        H5Pset_filter(plist_id, FILTER_BLOSC, H5Z_FLAG_OPTIONAL, numOfParams, cd_values);
//
//        //create write and close
//        hid_t space_id = H5Screate_simple(rank, dims, NULL);
//        hid_t dset_id = H5Dcreate2(obj_id, ds_name, type_id, space_id, H5P_DEFAULT, plist_id, H5P_DEFAULT);
//        H5Dclose(dset_id);
//
//        H5Pclose(plist_id);



    }

    void init_with_file(APRFile& parts_file,std::string name,bool apr_or_tree_){

        parts_name = name;
        apr_or_tree = apr_or_tree_;
        fileStructure = parts_file.get_fileStructure();
        fileStructure->create_time_point(0,apr_or_tree_,"t");

        if(apr_or_tree) {
            group_id = fileStructure->objectId;
        } else {
            group_id = fileStructure->objectIdTree;
        }

        int mdc_nelmts;
        size_t rdcc_nelmts;
        size_t rdcc_nbytes;
        double rdcc_w0;
        hid_t  fapl_idChunked;
        herr_t status;

        fapl_idChunked = H5Pcreate(H5P_FILE_ACCESS);

        status = H5Pget_cache (fapl_idChunked, &mdc_nelmts, &rdcc_nelmts,
                               &rdcc_nbytes, &rdcc_w0);

        rdcc_nbytes = 1000000*400;
        rdcc_nelmts = 1000000;

        status = H5Pset_cache (fapl_idChunked, mdc_nelmts, rdcc_nelmts,
                               rdcc_nbytes, rdcc_w0);


        set_up_read(group_id, parts_name.c_str());


    }

    void load_row(int level,int z,int x,LinearIterator& it){

        it.begin(level,z,x);
        parts_start = it.begin_index;
        parts_end = it.end();

        if ((parts_end - parts_start) > 0) {
            data.resize(parts_end - parts_start);
            //APRWriter::readData(parts_name.c_str(), group_id, data.data(),parts_start,parts_end);

            read_data(data.data(),parts_start,parts_end);

        }

        // ------------ decompress if needed ---------------------
        if (this->compressor.get_compression_type() > 0) {
            this->compressor.decompress( data,parts_start);
        }
    }

    void load_slice(int level,int z,LinearIterator& it){

        it.begin(level,z,0); //begining of slice
        parts_start = it.begin_index;
        it.begin(level,z,it.x_num(level)-1); //to end of slice
        parts_end = it.end();

        if ((parts_end - parts_start) > 0) {
            data.resize(parts_end - parts_start);
            read_data(data.data(),parts_start,parts_end);

        }


        // ------------ decompress if needed ---------------------
        if (this->compressor.get_compression_type() > 0) {
            this->compressor.decompress( data,parts_start);
        }
    }

    void write_slice(int level,int z,LinearIterator& it){

        it.begin(level,z,0); //begining of slice
        parts_start = it.begin_index;
        it.begin(level,z,it.x_num(level)-1); //to end of slice
        parts_end = it.end();

        if ((parts_end - parts_start) > 0) {
            //compress if needed
            if (this->compressor.get_compression_type() > 0){
                this->compressor.compress(data);
            }

            write_data(data.data(),parts_start,parts_end);

        }


    }



    DataType& operator[](LinearIterator& it) override {
        return data[it.current_index - parts_start];
    }


    LazyData(){};


    void close(){
        H5Sclose (memspace_id);
        H5Sclose (dataspace_id);

        H5Tclose(dataType);
        H5Dclose(data_id);
    }

private:

    hid_t data_id;

    hid_t memspace_id;
    hid_t dataspace_id;

    hid_t dataType;

    hsize_t dims;

    hsize_t offset;
    hsize_t count ;
    hsize_t stride;
    hsize_t block;

    void set_up_read(hid_t obj_id, const char* data_name) {

        data_id =  H5Dopen2(obj_id, data_name ,H5P_DEFAULT);

        dataType = H5Dget_type(data_id);

        stride = 1;
        block = 1;

        dataspace_id = H5Dget_space (data_id);
        H5Sselect_hyperslab (dataspace_id, H5S_SELECT_SET, &offset,
                             &stride, &count, &block);

    }

    void read_data( void* buff,uint64_t elements_start,uint64_t elements_end) {

        dims = elements_end - elements_start;

        offset = elements_start;
        count = dims;
#ifdef HAVE_OPENMP
#pragma omp critical
#endif
        {
            H5Sselect_hyperslab (dataspace_id, H5S_SELECT_SET, &offset,
                             &stride, &count, &block);

            memspace_id = H5Screate_simple (1, &dims, NULL);


            H5Dread(data_id, dataType, memspace_id, dataspace_id, H5P_DEFAULT, buff);
        }

    }

    void write_data( void* buff,uint64_t elements_start,uint64_t elements_end) {

        dims = elements_end - elements_start;

        offset = elements_start;
        count = dims;

#ifdef HAVE_OPENMP
#pragma omp critical
#endif
        {
            H5Sselect_hyperslab(dataspace_id, H5S_SELECT_SET, &offset,
                                &stride, &count, &block);

            memspace_id = H5Screate_simple(1, &dims, NULL);

            H5Dwrite(data_id, dataType, memspace_id, dataspace_id, H5P_DEFAULT, buff);
        }

    }

};


#endif //LIBAPR_LAZYDATA_HPP
