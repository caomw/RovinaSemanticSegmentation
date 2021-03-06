#include "libforest/classifiers.h"
#include "libforest/data.h"
#include "libforest/io.h"
#include "libforest/util.h"
#include <ios>
#include <iostream>
#include <string>
#include <cmath>

using namespace libf;

////////////////////////////////////////////////////////////////////////////////
/// Classifier
////////////////////////////////////////////////////////////////////////////////

void Classifier::classify(const DataStorage* storage, std::vector<int> & results) const
{
    // Clean the result set
    results.erase(results.begin(), results.end());
    results.resize(storage->getSize());
    
    // Classify each individual data point
    for (int i = 0; i < storage->getSize(); i++)
    {
        results[i] = classify(storage->getDataPoint(i));
    }
}

int Classifier::classify(const DataPoint* x) const
{
    // Get the class posterior
    std::vector<float> posterior;
    this->classLogPosterior(x, posterior);
    
    assert(posterior.size() > 0);
    
    int label = 0;
    float prob = posterior[0];
    const int size = static_cast<int>(posterior.size());
    
    for (int i = 1; i < size; i++)
    {
        if (posterior[i] > prob)
        {
            label = i;
            prob = posterior[i];
        }
    }
    
    return label;
}

////////////////////////////////////////////////////////////////////////////////
/// DecisionTree
////////////////////////////////////////////////////////////////////////////////

DecisionTree::DecisionTree()
{
    splitFeatures.reserve(LIBF_GRAPH_BUFFER_SIZE);
    thresholds.reserve(LIBF_GRAPH_BUFFER_SIZE);
    leftChild.reserve(LIBF_GRAPH_BUFFER_SIZE);
    histograms.reserve(LIBF_GRAPH_BUFFER_SIZE);
    // Add at least the root node with index 0
    addNode();
}

void DecisionTree::addNode()
{
    splitFeatures.push_back(0);
    thresholds.push_back(0);
    leftChild.push_back(0);
    histograms.push_back(std::vector<float>());
    multi_histograms.push_back(std::vector<std::vector<float> >());

}

int DecisionTree::splitNode(int node)
{
    // Make sure this is a valid node ID
    assert(0 <= node && node < static_cast<int>(splitFeatures.size()));
    // Make sure this is a former child node
    assert(leftChild[node] == 0);
    
    // Determine the index of the new left child
    const int leftNode = static_cast<int>(splitFeatures.size());
    
    // Add the child nodes
    addNode();
    addNode();
    
    // Set the child relation
    leftChild[node] = leftNode;
    
    return leftNode;
}

int DecisionTree::findLeafNode(const DataPoint* x) const
{
    // Select the root node as current node
    int node = 0;
    // Follow the tree until we hit a leaf node
    while (leftChild[node] != 0)
    {
        // Check the threshold
        if (x->at(splitFeatures[node]) < thresholds[node])
        {
            // Go to the left
            node = leftChild[node];
        }
        else
        {
            // Go to the right
            node = leftChild[node] + 1;
        }
    }
    return node;
}

void DecisionTree::classLogPosterior(const DataPoint* x, std::vector<float> & probabilities) const
{
    // Get the leaf node
    const int leafNode = findLeafNode(x);
    probabilities = getHistogram(leafNode);
}

void DecisionTree::multiClassLogPosterior(const DataPoint* x, std::vector<std::vector<float> > & probabilities) const
{
    // Get the leaf node
    const int leafNode = findLeafNode(x);
    probabilities = getMultiHistogram(leafNode);
}


void DecisionTree::read(std::istream& stream)
{
    // Write the attributes to the file
    readBinary(stream, splitFeatures);
    readBinary(stream, thresholds);
    readBinary(stream, leftChild);
    readBinary(stream, histograms);
    readBinary(stream, multi_histograms);
}

void DecisionTree::write(std::ostream& stream) const
{
    // Write the attributes to the file
    writeBinary(stream, splitFeatures);
    writeBinary(stream, thresholds);
    writeBinary(stream, leftChild);
    writeBinary(stream, histograms);
    writeBinary(stream, multi_histograms);
}

////////////////////////////////////////////////////////////////////////////////
/// RandomForest
////////////////////////////////////////////////////////////////////////////////

RandomForest::~RandomForest()
{
    for (size_t i = 0; i < trees.size(); i++)
    {
        delete trees[i];
    }
}

void RandomForest::classLogPosterior(const DataPoint* x, std::vector<float> & probabilities) const
{
    assert(getSize() > 0);
    trees[0]->classLogPosterior(x, probabilities);
    
    // Let the crowd decide
    for (size_t i = 1; i < trees.size(); i++)
    {
        // Get the probabilities from the current tree
        std::vector<float> currentHist;
        trees[i]->classLogPosterior(x, currentHist);
        
        // Accumulate the votes
        for (int  c = 0; c < currentHist.size(); c++)
        {
            probabilities[c] += currentHist[c];
        }
    }
}


void RandomForest::multiClassLogPosterior(const DataPoint* x, std::vector<std::vector<float> > & probabilities) const
{
    assert(getSize() > 0);
    trees[0]->multiClassLogPosterior(x, probabilities);
    
    // Let the crowd decide
    for (size_t i = 1; i < trees.size(); i++)
    {
        // Get the probabilities from the current tree
        std::vector<std::vector<float> > currentHist;
        trees[i]->multiClassLogPosterior(x, currentHist);
        
        // For each label layer
        for (int l = 0; l < currentHist.size(); l++){
            // Accumulate the votes
            for (int c = 0; c < currentHist[l].size(); c++)
            {
                probabilities[l][c] += currentHist[l][c];
            }
        }
    }
}

void RandomForest::write(std::ostream& stream) const
{
    // Write the number of trees in this ensemble
    writeBinary(stream, getSize());
    
    // Write the individual trees
    for (int i = 0; i < getSize(); i++)
    {
        getTree(i)->write(stream);
    }
}

void RandomForest::read(std::istream& stream)
{
    // Read the number of trees in this ensemble
    int size;
    readBinary(stream, size);
    
    // Read the trees
    for (int i = 0; i < size; i++)
    {
        DecisionTree* tree = new DecisionTree();
        tree->read(stream);
        addTree(tree);
    }
}

////////////////////////////////////////////////////////////////////////////////
/// BoostedRandomForest
////////////////////////////////////////////////////////////////////////////////

BoostedRandomForest::~BoostedRandomForest()
{
    for (size_t i = 0; i < trees.size(); i++)
    {
        delete trees[i];
    }
}


void BoostedRandomForest::write(std::ostream& stream) const
{
    // Write the number of trees in this ensemble
    writeBinary(stream, getSize());
    
    // Write the individual trees
    for (int i = 0; i < getSize(); i++)
    {
        // Write the weight
        writeBinary(stream, weights[i]);
        getTree(i)->write(stream);
    }
}

void BoostedRandomForest::read(std::istream& stream)
{
    // Read the number of trees in this ensemble
    int size;
    readBinary(stream, size);
    
    // Read the trees
    for (int i = 0; i < size; i++)
    {
        DecisionTree* tree = new DecisionTree();
        tree->read(stream);
        // Read the weight
        float weight;
        readBinary(stream, weight);
        addTree(tree, weight);
    }
}


void BoostedRandomForest::classLogPosterior(const DataPoint* x, std::vector<float> & probabilities) const
{
    assert(getSize() > 0);
    // Determine the number of classes by looking at a histogram
    trees[0]->classLogPosterior(x, probabilities);
    // Initialize the result vector
    const int C = static_cast<int>(probabilities.size());
    for (int c = 0; c < C; c++)
    {
        probabilities[c] = 0;
    }
    
    // Let the crowd decide
    for (size_t i = 0; i < trees.size(); i++)
    {
        // Get the resulting label
        const int label = trees[i]->classify(x);
        probabilities[label] += weights[i];
    }
}

void BoostedRandomForest::multiClassLogPosterior(const DataPoint* x, std::vector<std::vector<float> > & probabilities) const
{
    std::cerr << "This is not supported" << std::endl;
}