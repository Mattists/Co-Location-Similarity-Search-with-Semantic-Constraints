#include "Preprocessor.hpp"
#include "MatrixIO.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>


namespace fs = std::filesystem;
using namespace Eigen;
using namespace std;





/**
 *  Preprocesses a dataset and creates all individual adjacency matrices as well their self-instances (deltas).
 * 
 * @param datasetName The name of the dataset to preprocess.
 * @param directory The directory containing the dataset.
 * @param nameByEdgeType Whether to name the matrices by their edge types or by the first letters of the node types.
 * @param saveMatrixMarketFile Whether to save the matrices in the non-binary market format as well.
 * @param saveDeltaTextFile Whether to save the delta vectors in a non-binary file as well.
 */
template <typename T>
void preprocessDataset(const string& datasetName,
                       const string& directory,
                       const bool nameByEdgeType,
                       const bool saveMatrixMarketFile,
                       const bool saveDeltaTextFile) {

    // get the paths to the necessary files
    fs::path dataset_path = fs::path(directory) / datasetName;
    fs::path edge_types_path = fs::path(dataset_path) / "edge_types.csv";
    fs::path node_types_path = fs::path(dataset_path) / "node_types.csv";
    fs::path nodes_path = fs::path(dataset_path) / "nodes.csv";
    fs::path edges_path = fs::path(dataset_path) / "edges.csv";
    fs::path schema_path = fs::path(dataset_path) / "schema.csv";

    // check if all necessary files are present first, before loading
    ifstream edgeTypesCSV(edge_types_path);
    if (!edgeTypesCSV.is_open()) throw runtime_error("Missing edge types file: " + edge_types_path.string());
    ifstream nodeTypesCSV(node_types_path);
    if (!nodeTypesCSV.is_open()) throw runtime_error("Missing node types file: " + node_types_path.string());
    ifstream nodesCSV(nodes_path);
    if (!nodesCSV.is_open()) throw runtime_error("Missing nodeTypes file: " + nodes_path.string());
    ifstream edgesCSV(edges_path);
    if (!edgesCSV.is_open()) throw runtime_error("Missing edges file: " + edges_path.string());
    ifstream schemaCSV(schema_path);
    if (!schemaCSV.is_open()) throw runtime_error("Missing schema file: " + schema_path.string());


    string line;    // buffer for reading lines


    // ----------------- Load all type names and mappings -----------------

    // Get all node types in a map and vertexRow
    map<string, int> nodeTypeIDs;       // get node type ID from node type name
    vector<string> nodeTypeNames;       // get node type name from node type ID
    int nodeTypeID = 0;
    while (getline(nodeTypesCSV, line)) {
        nodeTypeIDs[line] = nodeTypeID;
        nodeTypeNames.push_back(line);
        nodeTypeID++;
    }
    nodeTypesCSV.close();

    // Get all edge types in a map and a vertexRow
    map<string, int> edgeTypesIDs;      // get edge type ID from edge type name
    vector<string> edgeTypesNames;      // get edge type name from edge type ID
    int edgeTypeID = 0;
    while (getline(edgeTypesCSV, line)) {
        edgeTypesIDs[line] = edgeTypeID;
        edgeTypesNames.push_back(line);
        edgeTypeID++;
    }
    edgeTypesCSV.close();

    // get the schema
    vector<pair<int, int>> nodeTypesOfEdge;     // get node type IDs from edge type ID
    while (getline(schemaCSV, line)){
        vector<string> chunks = split(line, ',');
        int srcType = stoi(chunks[0]);
        int dstType = stoi(chunks[1]);
        pair<int, int> schemaEdge = make_pair(srcType, dstType);
        nodeTypesOfEdge.push_back(schemaEdge);
    }
    schemaCSV.close();


    // ----------------- Load all nodes and edges -----------------

    // Get all the nodes
    vector<int> nodeTypes;          // get node type ID from node ID
    vector<int> typeSize;           // typeSize[i] = number of nodes of type i
    for(int i = 0; i < nodeTypeNames.size(); i++){
        typeSize.push_back(0);
    }
    while (getline(nodesCSV, line)){
        nodeTypeID = stoi(line);
        nodeTypes.push_back(nodeTypeID);
        typeSize[nodeTypeID] += 1;
    }
    nodesCSV.close();

    // Normalize node IDs, i.e., first node of each type has ID 0
    vector<int> newNodeIDs;             // get new node ID from old node ID
    vector<int> nodeCountersByType;     // nodeCountersByType[i] = number of nodes of type i already processed
    for (int i = 0; i < nodeTypeNames.size(); i++) {
        nodeCountersByType.push_back(0);
    }
    for(int nodeType : nodeTypes){
        newNodeIDs.push_back(nodeCountersByType[nodeType]);
        nodeCountersByType[nodeType] += 1;
    }

    // Get all the edges
    vector<pair<int, int>> edges;       // assigns edge[i] its source and destination node ID
    vector<int> edgeTypes;              // assigns edge[i] its edge type ID
    while (getline(edgesCSV, line)){
        vector<string> chunks = split(line, ',');
        edges.emplace_back(stoi(chunks[0]), stoi(chunks[2]));
        edgeTypes.push_back(stoi(chunks[1]));
    }
    edgesCSV.close();


    // ----------------- Create the individual matrices -----------------

    // First, create empty triplet vectors for all edge types
    vector<vector<Eigen::Triplet<T>>> matrixTriplets;
    for(int i = 0; i < edgeTypesNames.size(); i++){
        vector<Eigen::Triplet<T>> tripletVector;
        matrixTriplets.push_back(tripletVector);
    }

    // Second, fill triplet vectors with the respective edges
    for(int i = 0; i < edges.size(); i++){
        edgeTypeID = edgeTypes[i];
        int newSrcID = newNodeIDs[edges[i].first];
        int newDstID = newNodeIDs[edges[i].second];
        Eigen::Triplet<T> triplet(newSrcID, newDstID, 1);
        matrixTriplets[edgeTypeID].push_back(triplet);
    }

    // Third, create the individual matrices from the triplet vectors
    vector<SparseMatrix<T, RowMajor>> matrices;
    for(int i = 0; i < edgeTypesNames.size(); i++){
        int rows = typeSize[nodeTypesOfEdge[i].first];
        int cols = typeSize[nodeTypesOfEdge[i].second];
        SparseMatrix<T, RowMajor> matrix(rows, cols);
        matrix.setFromTriplets(matrixTriplets[i].begin(), matrixTriplets[i].end());
        matrices.push_back(matrix);
    }


    // Fourth, store the matrices
    fs::path matrix_dir_path = dataset_path / "matrices";
    fs::create_directory(matrix_dir_path);
    for(int i = 0; i < edgeTypesNames.size(); i++){
        string matrixName;
        if (nameByEdgeType) {
            // Remove all whitespace variants from the end of the string, add 1 as the length is required and not the end index
            matrixName = edgeTypesNames[i].substr(0, edgeTypesNames[i].find_last_not_of(" \t\n\r\f\v") + 1);
        } else {
            string nodeTypeSrc = nodeTypeNames[nodeTypesOfEdge[i].first];
            string nodeTypeDst = nodeTypeNames[nodeTypesOfEdge[i].second];
            matrixName += char(toupper(int(nodeTypeSrc[0])));
            matrixName += char(toupper(int(nodeTypeDst[0])));
        }
        fs::path matrix_path = matrix_dir_path / (matrixName + ".mtx");
        if (saveMatrixMarketFile) {
            saveMarket(matrices[i], matrix_path);
        }
        saveMatrixBinary<T>(matrices[i], matrix_path);
        createAndStoreDeltas<T>(matrices[i],
                                matrixName,
                                matrix_dir_path.string(),
                                nameByEdgeType,
                                "",
                                false,
                                saveDeltaTextFile);
    }
}


/**
 * Generates the partial commuting matrix (left half of the matrix) for a given dataset and meta-path.
 * 
 * @param datasetName The name of the dataset.
 * @param directory The directory containing the dataset.
 * @param metaPath The meta path for which to generate the partial commuting matrix. Called matrix name in other functions.
 * @param nameByEdgeType Whether to name the created matrix by their edge types or by the first letters of the connected node types.
 * @param saveMatrixMarketFile Whether to save the created matrix in the non-binary market format as well.
 * @param saveDeltaTextFile Whether to save the created delta vector in a non-binary file as well.
 */
template <typename T>
void generatePartialCommutingMatrix(const string& datasetName,
                                    const string& directory,
                                    const string& metaPath,
                                    const bool nameByEdgeType,
                                    const bool saveMatrixMarketFile,
                                    const bool saveDeltaTextFile) {

    cout << endl << "Generating partial commuting matrix for meta-path " << metaPath << " in dataset: " << datasetName
         << endl << endl;

    fs::path dataset_path = fs::path(directory) / datasetName;
    fs::path matrix_dir_path = dataset_path / "matrices";

    vector<string> matrixNames;
    if (nameByEdgeType) {
        // '#' is assumed to be the character to separate different edges in a meta path when nameByEdgeType is true 
        //since '_' is used in naming  for some datasets, e.g. "find_site" and the '_' is used to denote the reverse edge "^-1"
        matrixNames = split(metaPath, '#');
    } else {    
        // each substring of length 2 in the meta path is a matrix name
        for (int i = 0; i < metaPath.size() - 1; i++) {
            matrixNames.push_back(metaPath.substr(i, 2));
        }
    }

    vector<Eigen::SparseMatrix<T, RowMajor>> matrices;
    for (auto matrixName : matrixNames) {
        fs::path matrix_path = matrix_dir_path / (matrixName + ".mtx");

        Eigen::SparseMatrix<T, RowMajor> matrix;
        bool reverseMatrix = false;
        if (!loadMatrix<T>(matrix_path, matrix)) {
            // remove the "^-1" in the end of matrixName if nameByEdgeType is True. Otherwise simply reverse the matrixName
            if (nameByEdgeType) {
                matrixName.erase(matrixName.size() - 3);
            } else {
                reverse(matrixName.begin(), matrixName.end());
            }
            matrix_path = matrix_dir_path / (matrixName + ".mtx");
            reverseMatrix = true;

            if (!loadMatrix<T>(matrix_path, matrix)) {
                throw runtime_error("Can not open commuting matrix file: " + matrix_path.string());
            }
        }

        if (reverseMatrix) {
            matrix = matrix.transpose();
            matrix.makeCompressed();
        }
        matrices.push_back(matrix);
    }

    // create the full commuting matrix
    Eigen::SparseMatrix<T, RowMajor> fullCommutingMatrix = matrices[0];
    for (int i = 1; i < matrices.size(); i++) {
        fullCommutingMatrix = fullCommutingMatrix * matrices[i];
    }

    const fs::path fullCommutingMatrixPath = matrix_dir_path / (metaPath + ".mtx");
    if (saveMatrixMarketFile) {
        saveMarket(fullCommutingMatrix, fullCommutingMatrixPath);
    }
    saveMatrixBinary<T>(fullCommutingMatrix, fullCommutingMatrixPath);
    createAndStoreDeltas<T>(fullCommutingMatrix,
                            metaPath,
                            matrix_dir_path.string(),
                            nameByEdgeType,
                            "",
                            false,
                            saveDeltaTextFile);
}


/**
 * Generates the partial commuting matrix (left half of the matrix) for a given dataset and multiple meta-paths.
 *  
 * @param datasetName The name of the dataset.
 * @param directory The directory containing the dataset.
 * @param metaPatha The meta paths for which to generate the partial commuting matrix. Called matrix name in other functions.
 * @param nameByEdgeType Whether to name the created matrices by their edge types or by the first letters of the connected node types.
 * @param saveMatrixMarketFile Whether to save the created matrices in the non-binary market format as well.
 * @param saveDeltaTextFile Whether to save the created delta vectors in a non-binary file as well.
 */
template <typename T>
void generatePartialCommutingMatrix(const string& datasetName,
                                    const string& directory,
                                    const vector<string>& metaPaths,
                                    const bool nameByEdgeType,
                                    const bool saveMatrixMarketFile,
                                    const bool saveDeltaTextFile) {

    for (const string& metaPath: metaPaths) {
        generatePartialCommutingMatrix<T>(datasetName,
                                          directory,
                                          metaPath,
                                          nameByEdgeType,
                                          saveMatrixMarketFile,
                                          saveDeltaTextFile);
    }
}



// ----------------------------------- Instantiation of template functions --------------------------


template void preprocessDataset<unsigned long long>(const string&,
                                                    const string&,
                                                    const bool,
                                                    const bool,
                                                    const bool);


template void preprocessDataset<long double>(const string&,
                                             const string&,
                                             const bool,
                                             const bool,
                                             const bool);


template void generatePartialCommutingMatrix<unsigned long long> (const string&,
                                                                   const string&,
                                                                   const string&,
                                                                   const bool,
                                                                   const bool,
                                                                   const bool);


template void generatePartialCommutingMatrix<long double>(const string&,
                                                          const string&,
                                                          const string&,
                                                          const bool,
                                                          const bool,
                                                          const bool);


template void generatePartialCommutingMatrix<unsigned long long>(const string&,
                                                                 const string&,
                                                                 const vector<string>&,
                                                                 const bool,
                                                                 const bool,
                                                                 const bool);


template void generatePartialCommutingMatrix<long double>(const string&,
                                                          const string&,
                                                          const vector<string>&,
                                                          const bool,
                                                          const bool,
                                                          const bool);
