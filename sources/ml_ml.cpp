/*
 * ml-lib, a machine learning library for Max and Pure Data
 * Copyright (C) 2013 Carnegie Mellon University
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ml_ml.h"

#include <string>

namespace ml
{
    
#pragma mark - Constants
    const std::string k_model_extension = ".model";
    const std::string k_data_extension = ".data";
    
#pragma mark - Utility methods
    
    const std::string get_symbol_as_string(const t_symbol *symbol)
    {
        const char *c_string = flext::GetAString(symbol);
        std::string cpp_string = c_string != NULL ? c_string : "";
        
        return cpp_string;
    }
    
    const std::string get_file_extension_from_path(const std::string &path_)
    {
        std::string extension;
        std::string path = path_;
        
        size_t sep = path.find_last_of("\\/");
        
        if (sep != std::string::npos)
        {
            path = path.substr(sep + 1, path.size() - sep - 1);
        }
        
        size_t dot = path.find_last_of(".");
        
        if (dot != std::string::npos)
        {
            extension = path.substr(dot, path.size() - dot);
        }
        
        extension = extension == "." ? "" : extension;
        
        return  extension;
    }
    
    void get_data_file_paths(const std::string &supplied_path, std::string &data_path, std::string &model_path)
    {
        std::string extension = get_file_extension_from_path(supplied_path);
        
        if (extension == k_model_extension)
        {
            model_path = supplied_path;
        }
        else if (extension == k_data_extension)
        {
            data_path = supplied_path;
        }
        else
        {
            data_path = supplied_path + k_data_extension;
            model_path = supplied_path + k_model_extension;
        }
    }
    
    
#pragma mark - ml implementation
    
    void ml::set_num_inputs(uint8_t num_inputs)
    {
        if (num_inputs < 0)
        {
            error("number of inputs must be greater than zero");
        }
        
        bool success = false;
        const ml_data_type data_type = get_data_type();
        
        if (data_type == LABELLED_CLASSIFICATION)
        {
            success = classificationData.setNumDimensions(num_inputs);
        }
        else if (data_type == LABELLED_REGRESSION)
        {
            success = regressionData.setInputAndTargetDimensions(num_inputs, regressionData.getNumTargetDimensions());
        }
        else if (data_type == LABELLED_TIME_SERIES_CLASSIFICATION)
        {
            success = timeSeriesClassificationData.setNumDimensions(num_inputs);
        }
        else if (data_type == UNLABELLED_CLASSIFICATION)
        {
            success = unlabelledData.setNumDimensions(num_inputs);
        }
        
        if (success == false)
        {
            error("unable to set input or target dimensions");
            return;
        }
    }
    
    ml::ml()
    : currentLabel(0), probs(false), recording(false)
    {
        set_data_type(default_data_type);
        set_num_inputs(defaultNumInputDimensions);
        AddOutAnything("general purpose outlet");
    }
    
    void ml::set_scaling(bool scaling)
    {
        bool success = false;
        GRT::MLBase &mlBase = get_MLBase_instance();
        
        success = mlBase.enableScaling(scaling);
        
        if (success == false)
        {
            error("unable to set scaling, hint: should be 0 or 1");
        }
    }
    
    void ml::get_scaling(bool &scaling) const
    {
        const GRT::MLBase &mlBase = get_MLBase_instance();
        
        scaling = mlBase.getScalingEnabled();
    }
    
    void ml::set_probs(bool probs)
    {
        this->probs = probs;
    }
    
    void ml::get_probs(bool &probs) const
    {
        probs = this->probs;
    }
    
    void ml::add(int argc, const t_atom *argv)
    {
        if (argc < 2)
        {
            error("invalid input length, must contain at least 2 values");
            return;
        }
        
        GRT::UINT numInputDimensions = 0;
        GRT::UINT numOutputDimensions = 1;
        const ml_data_type data_type = get_data_type();
        
        if (data_type == LABELLED_CLASSIFICATION)
        {
            numInputDimensions = classificationData.getNumDimensions();
        }
        else if (data_type == LABELLED_REGRESSION)
        {
            numInputDimensions = regressionData.getNumInputDimensions();
            numOutputDimensions = regressionData.getNumTargetDimensions();
        }
        else if (data_type == LABELLED_TIME_SERIES_CLASSIFICATION)
        {
            numInputDimensions = timeSeriesClassificationData.getNumDimensions();
        }
        else if (data_type == UNLABELLED_CLASSIFICATION)
        {
            numInputDimensions = unlabelledData.getNumDimensions();
        }
        else
        {
            error("unhandled data_type:" + std::to_string(data_type));
            return;
        }
        
        GRT::UINT combinedVectorSize = numInputDimensions + numOutputDimensions;
        
        if (argc < 0 || (unsigned)argc != combinedVectorSize)
        {
            numInputDimensions = argc - numOutputDimensions;
            
            if (numInputDimensions < 1)
            {
                error("invalid input length, expected at least " + std::to_string(numOutputDimensions + 1));
                return;
            }
            post("new input vector size, adjusting num_inputs to " + std::to_string(numInputDimensions));
            set_num_inputs(numInputDimensions);
        }
        
        GRT::VectorDouble inputVector(numInputDimensions);
        GRT::VectorDouble targetVector(numOutputDimensions);
        
        for (uint32_t index = 0; index < (unsigned)argc; ++index)
        {
            float value = GetAFloat(argv[index]);
            
            if (index < numOutputDimensions)
            {
                targetVector[index] = value;
            }
            else
            {
                inputVector[index - numOutputDimensions] = value;
            }
        }
        
        if (data_type == LABELLED_CLASSIFICATION || data_type == LABELLED_TIME_SERIES_CLASSIFICATION)
        {
            GRT::UINT label = (GRT::UINT)targetVector[0];
            
            if ((double)label != targetVector[0])
            {
                error("class label must be a positive integer");
                return;
            }
            
            if (label == 0)
            {
                error("class label must be non-zero");
                return;
            }
            
            if (data_type == LABELLED_CLASSIFICATION)
            {
                classificationData.addSample((GRT::UINT)targetVector[0], inputVector);
            }
            else if (data_type == LABELLED_TIME_SERIES_CLASSIFICATION)
            {
                if (recording)
                {
                    // allow label to be changed on-the-fly without explicitly toggling "record"
                    if (label != currentLabel)
                    {
                        record_(false);
                        record_(true);
                    }
                    currentLabel = label;
                    timeSeriesData.push_back(inputVector);
                }
                else
                {
                    error("cannot add time series data if recording is off, send 'record 1' to start recording");
                }
            }
        }
        else if (data_type == LABELLED_REGRESSION)
        {
            regressionData.addSample(inputVector, targetVector);
        }
    }
    
    void ml::record_(bool state)
    {
        const ml_data_type data_type = get_data_type();
        
        if (data_type != LABELLED_TIME_SERIES_CLASSIFICATION)
        {
            error("record method only valid for time series data");
            return;
        }
        
        recording = state;
        
        if (recording == false && currentLabel != 0 && timeSeriesData.getNumRows() > 0)
        {
            timeSeriesClassificationData.addSample(currentLabel, timeSeriesData);
        }
        timeSeriesData.clear();
        currentLabel = 0;
    }
    
    void ml::record(bool state)
    {
        record_(state);
        std::string record_state = recording ? "on" : "off";
        post("recording: " + record_state);
    }
    
    void ml::write(const t_symbol *path) const
    {
        bool success = false;
        t_atom a_success;
        SetInt(a_success, success);
        const ml_data_type data_type = get_data_type();
        std::string file_path = get_symbol_as_string(path);
        const GRT::MLBase &mlBase = get_MLBase_instance();
        
        if (
            (data_type == LABELLED_REGRESSION && regressionData.getNumSamples() == 0) ||
            (data_type == LABELLED_CLASSIFICATION && classificationData.getNumSamples() == 0) ||
            (data_type == LABELLED_TIME_SERIES_CLASSIFICATION && timeSeriesClassificationData.getNumSamples() == 0) ||
            (data_type == UNLABELLED_CLASSIFICATION && unlabelledData.getNumSamples() == 0)
            )
        {
            error("no observations added, use 'add' to add training data");
            ToOutAnything(1, s_write, 1, &a_success);
            return;
        }
        
        if (check_empty_with_error(file_path))
        {
            return;
        }
        
        std::string dataset_file_path;
        std::string model_file_path;
        
        get_data_file_paths(file_path, dataset_file_path, model_file_path);
        
        if (!dataset_file_path.empty())
        {
            success = write_specialised_dataset(dataset_file_path);
        
            if (!success)
            {
                error("unable to write training data to path: " + dataset_file_path);
            }
        }
        
        if (!model_file_path.empty())
        {
            if (mlBase.getTrained())
            {
                success = mlBase.saveModelToFile(model_file_path);
                
                if (!success)
                {
                    error("unable to write model to path: " + model_file_path);
                }
            }
            else if (get_file_extension_from_path(file_path) == k_model_extension)
            {
                error("model not trained, use 'train' to train a model");
            }
        }
       
        SetInt(a_success, success);
        ToOutAnything(1, s_write, 1, &a_success);
    }
    
    void ml::read(const t_symbol *path)
    {
        bool success = false;
        t_atom a_success;
        SetInt(a_success, success);
        GRT::MLBase &mlBase = get_MLBase_instance();

        std::string file_path = get_symbol_as_string(path);
        
        if (check_empty_with_error(file_path))
        {
            return;
        }
        
        std::string dataset_file_path;
        std::string model_file_path;
        
        get_data_file_paths(file_path, dataset_file_path, model_file_path);

        if (!dataset_file_path.empty())
        {
            success = read_specialised_dataset(dataset_file_path);
            
            if (!success)
            {
                error("unable to read training data from path: " + dataset_file_path);
            }
        }
        
        if (!model_file_path.empty())
        {
            success = mlBase.loadModelFromFile(model_file_path);
            
            if (!success)
            {
                error("unable to read model from path: " + model_file_path);
            }
        }
        
        SetInt(a_success, success);
        ToOutAnything(1, s_read, 1, &a_success);
    }
    
    void ml::clear()
    {
        t_atom status;
        GRT::MLBase &mlBase = get_MLBase_instance();
        
        mlBase.clear();
        
        regressionData.clear();
        classificationData.clear();
        timeSeriesClassificationData.clear();
        unlabelledData.clear();
        
        SetBool(status, true);
        ToOutAnything(1, s_clear, 1, &status);
    }
    
    void ml::train()
    {
        error("function not implemented");
    }
    
    void ml::map(int argc, const t_atom *argv)
    {
        error("function not implemented");
    }
    
    void ml::usage()
    {
        post(ML_LINE_SEPARATOR);
        post("Attributes:");
        post(ML_LINE_SEPARATOR);
        post("scaling:\tinteger (0 or 1) sets whether values are automatically scaled (default 1)");
        post("probs:\tinteger (0 or 1) determing whether probabilities are sent from the right outlet");
        post(ML_LINE_SEPARATOR);
        post("Methods:");
        post(ML_LINE_SEPARATOR);
        post("add:\tlist comprising a class id followed by n features; <class> <feature 1> <feature 2> etc");
        post("write:\twrite training examples, first argument gives path to write location");
        post("read:\tread training examples, first argument gives path to the read location");
        post("train:\ttrain the MLP based on vectors added with 'add'");
        post("clear:\tclear the stored training data and model");
        post("map:\tgive the regression value for the input feature vector");
        post("help:\tpost this usage statement to the console");
        post(ML_LINE_SEPARATOR);
    }
    
    void ml::any(const t_symbol *s, int argc, const t_atom *argv)
    {
        error("messages with the selector '" + std::string(GetString(s)) + "' are not supported");
    }
    
    std::string ml::get_grt_version()
    {
        GRT::MLBase &mlBase = get_MLBase_instance();
        return mlBase.getGRTVersion();
    }
    
    void ml::setup(t_classid c)
    {
        FLEXT_CADDATTR_SET(c, "scaling", set_scaling);
        FLEXT_CADDATTR_SET(c, "probs", set_probs);
        
        FLEXT_CADDATTR_GET(c, "scaling", get_scaling);
        FLEXT_CADDATTR_GET(c, "probs", get_probs);
        
        FLEXT_CADDMETHOD(c, 0, any);
        FLEXT_CADDMETHOD_(c, 0, "add", add);
        FLEXT_CADDMETHOD_(c, 0, "record", record);
        FLEXT_CADDMETHOD_(c, 0, "write", write);
        FLEXT_CADDMETHOD_(c, 0, "read", read);
        FLEXT_CADDMETHOD_(c, 0, "train", train);
        FLEXT_CADDMETHOD_(c, 0, "clear", clear);
        FLEXT_CADDMETHOD_(c, 0, "map", map);
        FLEXT_CADDMETHOD_(c, 0, "help", usage);
    }
    
    void ml::set_data_type(ml_data_type data_type)
    {
        if (data_type > MLP_NUM_DATA_TYPES)
        {
            error("invalid data type: %d" + std::to_string(data_type));
            return;
        }
        this->data_type = data_type;
    }
    
    ml_data_type ml::get_data_type() const
    {
        return data_type;
    }
    
    bool ml::check_empty_with_error(std::string &string) const
    {
        if (string.empty())
        {
            error("path string is empty");
            return true;
        }
        return false;
    }
    
#pragma mark - Main function
#ifdef BUILD_AS_LIBRARY
    static void main()
    {
        flext::post(ML_LINE_SEPARATOR);
        flext::post("%s - machine learning library for Max and Pure Data", ML_NAME);
        flext::post("version " ML_VERSION " (c) 2013 Carnegie Mellon University");
        flext::post(ML_LINE_SEPARATOR);
        
        // call the objects' setup routines
        FLEXT_SETUP(ml_svm);
        FLEXT_SETUP(ml_adaboost);
        FLEXT_SETUP(ml_dtw);
        FLEXT_SETUP(ml_hmm);
        FLEXT_SETUP(ml_mlp);
        FLEXT_SETUP(ml_linreg);
        FLEXT_SETUP(ml_logreg);
        FLEXT_SETUP(ml_peak);
        FLEXT_SETUP(ml_minmax);
        FLEXT_SETUP(ml_anbc);
        FLEXT_SETUP(ml_softmax);
        FLEXT_SETUP(ml_randforest);
        FLEXT_SETUP(ml_mindist);
//        FLEXT_SETUP(ml_lda);
        FLEXT_SETUP(ml_knn);
        FLEXT_SETUP(ml_gmm);
        FLEXT_SETUP(ml_dtree);
    }
#endif
    
#pragma mark - Global constants
    
    const t_symbol *ml::s_train = flext::MakeSymbol("train");
    const t_symbol *ml::s_clear = flext::MakeSymbol("clear");
    const t_symbol *ml::s_read = flext::MakeSymbol("read");
    const t_symbol *ml::s_write = flext::MakeSymbol("write");
    const t_symbol *ml::s_probs = flext::MakeSymbol("probs");
    const t_symbol *ml::s_error = flext::MakeSymbol("error");
    
} // namespace ml


#ifdef BUILD_AS_LIBRARY
FLEXT_LIB_SETUP(ml, ml::main)
#endif
