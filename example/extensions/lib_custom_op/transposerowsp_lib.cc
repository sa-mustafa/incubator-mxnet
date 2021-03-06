/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * Copyright (c) 2020 by Contributors
 * \file transsparse_lib.cc
 * \brief Sample 2D transpose custom operator.
 */

#include <iostream>
#include "lib_api.h"

void transpose(MXTensor src, MXTensor dst, OpResource res) {
  MXSparse* A = src.data<MXSparse>();
  MXSparse* B = dst.data<MXSparse>(); 

  std::vector<int64_t> shape = src.shape;
  int64_t h = shape[0];
  int64_t w = shape[1];
  if(src.stype == kRowSparseStorage) {
    // Keys of the map is the row index of transposed tensors.
    // Values of the map is the rows which have non-zero elements.    
    std::map<int, std::vector<float>> mp;
    float *Aval = (float*) (A->data);
    for(int i = 0; i < A->data_len; i++) {
      int row = i / w;
      int col = i % w;
      row = A->indices[row];
      if(Aval[i] != 0) {
        if(mp.find(col) == mp.end()) {
          mp[col] = std::vector<float>(h, 0);
          mp[col][row] = Aval[i];
        }
        else {
          mp[col][row] = Aval[i];
        }
      }
    }

    // Alloc memory for output tensors.
    res.alloc_sparse(B, 0, mp.size());
    float *Bval = (float*) (B->data);
    int didx = 0, iidx = 0;
    for(auto i : mp) {
      B->indices[iidx++] = i.first;
      for(auto j : i.second) {
        Bval[didx++] = j;
      }
    }
  }
}

MXReturnValue forward(std::map<std::string, std::string> attrs,
                      std::vector<MXTensor> inputs,
                      std::vector<MXTensor> outputs,
                      OpResource res) {
  // The data types and storage types of inputs and outputs should be the same.
  if(inputs[0].dtype != outputs[0].dtype || inputs[0].stype != outputs[0].stype) {
    std::cout << "Error! Expected all inputs and outputs to be the same type."
              << "Found input storage type:" << inputs[0].stype
              << " Found output storage type:" << outputs[0].stype
              << " Found input data type:" << inputs[0].dtype
              << " Found output data type:" << outputs[0].dtype << std::endl;
    return MX_FAIL;
  }
  transpose(inputs[0], outputs[0], res);
  return MX_SUCCESS;
}

MXReturnValue backward(std::map<std::string, std::string> attrs,
                       std::vector<MXTensor> inputs,
                       std::vector<MXTensor> outputs,
                       OpResource res) {
  return MX_SUCCESS;
}

MXReturnValue parseAttrs(std::map<std::string, std::string> attrs, int* num_in, int* num_out) {
  *num_in = 1;
  *num_out = 1;
  return MX_SUCCESS;
}

MXReturnValue inferType(std::map<std::string, std::string> attrs,
                        std::vector<int> &intypes,
                        std::vector<int> &outtypes) {
  // validate inputs
  if (intypes.size() != 1) {
    std::cout << "Expected 1 inputs to inferType" << std::endl;
    return MX_FAIL;
  }
  if (intypes[0] != kFloat32) {
    std::cout << "Expected input to have float32 type" << std::endl;
    return MX_FAIL;
  }

  outtypes[0] = intypes[0];
  return MX_SUCCESS;
}

MXReturnValue inferSType(std::map<std::string, std::string> attrs,
                        std::vector<int> &instypes,
                        std::vector<int> &outstypes) {
  if (instypes[0] != kRowSparseStorage) {
    std::cout << "Expected storage type is kRowSparseStorage" << std::endl;
    return MX_FAIL;
  }
  outstypes[0] = instypes[0];
  return MX_SUCCESS;
}

MXReturnValue inferShape(std::map<std::string, std::string> attrs,
                         std::vector<std::vector<unsigned int>> &inshapes,
                         std::vector<std::vector<unsigned int>> &outshapes) {
  // validate inputs
  if (inshapes.size() != 1) {
    std::cout << "Expected 1 inputs to inferShape" << std::endl;
    return MX_FAIL;
  }

  outshapes[0].push_back(inshapes[0][1]);
  outshapes[0].push_back(inshapes[0][0]);
  return MX_SUCCESS;
}

REGISTER_OP(my_transposerowsp)
.setForward(forward, "cpu")
.setBackward(backward, "cpu")
.setParseAttrs(parseAttrs)
.setInferType(inferType)
.setInferSType(inferSType)
.setInferShape(inferShape);

/* ------------------------------------------------------------------------- */

class MyStatefulTransposeRowSP : public CustomStatefulOp {
 public:
  explicit MyStatefulTransposeRowSP(int count) : count(count) {}

  MXReturnValue Forward(std::vector<MXTensor> inputs,
                        std::vector<MXTensor> outputs,
                        OpResource op_res) {
    std::cout << "Info: keyword + number of forward: " << ++count << std::endl;
    std::map<std::string, std::string> attrs;
    return forward(attrs, inputs, outputs, op_res);
  }

  MXReturnValue Backward(std::vector<MXTensor> inputs,
                         std::vector<MXTensor> outputs,
                         OpResource op_res) {
    std::map<std::string, std::string> attrs;
    return backward(attrs, inputs, outputs, op_res);
  }

 private:
  int count;
};

MXReturnValue createOpState(std::map<std::string, std::string> attrs,
                            CustomStatefulOp** op_inst) {
  // testing passing of keyword arguments
  int count = attrs.count("test_kw") > 0 ? std::stoi(attrs["test_kw"]) : 0;
  // creating stateful operator instance
  *op_inst = new MyStatefulTransposeRowSP(count);
  std::cout << "Info: stateful operator created" << std::endl;
  return MX_SUCCESS;
}

REGISTER_OP(my_state_transposerowsp)
.setParseAttrs(parseAttrs)
.setInferType(inferType)
.setInferSType(inferSType)
.setInferShape(inferShape)
.setCreateOpState(createOpState, "cpu");

MXReturnValue initialize(int version) {
  if (version >= 10400) {
    std::cout << "MXNet version " << version << " supported" << std::endl;
    return MX_SUCCESS;
  } else {
    std::cout << "MXNet version " << version << " not supported" << std::endl;
    return MX_FAIL;
  }
}
