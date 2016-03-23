#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <random>
#include <cstdlib>
#include <utility>

std::vector< std::vector<double> > dataPointsVector;
std::vector< std::vector<double> > sample;
std::vector< std::vector<double> > greedyMedoids;
std::vector< std::vector<double> > medoids_current;
std::vector<double> v;
std::vector<int> output;
std::ofstream myfile;
int dataPoints = 0;
int dimensions = 0;
int totalData = 0;



double manhattanSegDis(std::vector<double> *x1 , std::vector<double> *x2);
std::vector< std::vector<double> > randomInitialSample(std::vector< std::vector<double> > *dataPointsVector, int size);
inline double closed_interval_rand(double x0, double x1)
{   srand(time(NULL));
    return x0 + (x1 - x0) * rand() / ((double) RAND_MAX);
}

std::vector< std::vector<double> > greedySample(std::vector< std::vector<double> > *dataPointsVector, int size);

std::vector< std::vector<double> > nearPoints(std::vector< std::vector<double> > *dataPointsVector, std::vector<double> *medoid, double radius);

std::vector< std::vector<int> > findDimensions(int k, int l,std::vector< std::vector< std::vector<double> > > *L , std::vector< std::vector<double> > *medoids);

std::vector<double> avgDist(std::vector< std::vector<double> > *Li,  std::vector<double>  *medoid);

double manhattanSegDisDim(std::vector<double> *x1 , std::vector<double> *x2, int numDim);

std::vector<std::vector < std::vector<double> > > assignPoints( std::vector< std::vector<int> > *dimensions, std::vector< std::vector<double> > *medoids);

double manhattanSegDisWithDim( std::vector<double>  *point , std::vector<double>  *medoid, std::vector<int>  *dimension   );


bool pairCompare(const std::pair<double, int>& firstElem, const std::pair<double, int>& secondElem) {
    return firstElem.first < secondElem.first;
    
}

int main()
{   std::cout.precision(15);
   
    int n_cluster = 0;
    int const_a = 50;
    int const_b = 2;
    int l;
    
    
    myfile.open ("/Users/Mr.Pandya/Documents/HiPC/Program/Program/bits_pilani_me_cs_phase1.csv"); // This will open file in the same directory as of the code

    //std::ifstream  data("smallerds.csv");
    // Please replace following input data path by your's file path
    std::ifstream  data("/Users/Mr.Pandya/Documents/HiPC/Program/Program/HiPC2015_IntelData_40k.csv");
    
    std::string line;
    while(std::getline(data,line))
    {   dataPoints++;
        v.clear();
        std::stringstream  lineStream(line);
        std::string        cell;
        
        while(std::getline(lineStream,cell,','))
        {
            //std::cout << cell;
            v.push_back(std::stod(cell));
            totalData++;
        }
        dataPointsVector.push_back(v);
        output.push_back(-1); // Initialization of output vector
    }
    dimensions = totalData / dataPoints; // Here we have fixed number of dimension for all data points
    std::cout << "Total dataPoints : " << dataPoints << '\n';
    std::cout << "Total dimensions : " << dimensions << '\n';
    // Tesing input
    // std::vector<double> test1 = dataPointsVector.at(9);
    //std::vector<double> test2 = dataPointsVector.at(21);
    //std::cout << std::fixed << manhattanSegDis(&test1, &test2);
    // for( double data: test){
    
    //     std::cout << std::fixed << data << " ";
    // }
    std::cout << "Enter initial number of medoids: ";
    std::cin >> n_cluster;
    std::cout << "Enter the value for l: ";
    std::cin >> l;
    sample = randomInitialSample(&dataPointsVector,n_cluster*const_a);
    
    std::cout<<"Random Initial Sample Done ! \n";
    
    
    //  std::vector<double> test = sample.at(n_cluster*const_a );
    
    // for( double data: test){
    
    //       std::cout << std::fixed << data << " ";
    //  }
    
    greedyMedoids = greedySample(&sample,n_cluster*const_b);  // This will sample const_b*n_cluster
    std::cout<<"Greedy Sample done! \n";
    
    
    //std::vector<double> medoid1 = greedyMedoids.at(n_cluster*const_b - 1);
    //for(double val : medoid1){
    //    std::cout << std::fixed <<val ;
    //}
    //std::cout << " \n";
    
    medoids_current = greedySample(&greedyMedoids,n_cluster); // This will produce final sample of size n_cluster
    
    std::cout <<"Finding medoids done \n";
//    for ( int iterate_medoids = 0; iterate_medoids < n_cluster; iterate_medoids++) {
//        std::vector<double> medoid = medoids.at(iterate_medoids);
//        std::cout << "Medoid " << iterate_medoids << " : ";
//        for ( double data : medoid) {
//            std::cout << std::fixed << data ;
//        }
//        std::cout << '\n';
//    }
    
    std::vector< std::vector< std::vector<double> > > Li;
    
   // char terminate  = 'N';
   // while (terminate != 'Y') {
        int medoid_size = medoids_current.size();
        double newDist = 1000.00 ;
        double oldDist = 100000.00;
        // Make these loops based on int counter
        for (int iter1 = 0; iter1 < medoid_size ; iter1++) {
            std::vector<double> medoid = medoids_current.at(iter1);
            
          
            for( int iter2 =0 ; iter2 < medoid_size ; iter2++ ){
                if (iter1 != iter2) {
                    std::vector<double>  data = medoids_current.at(iter2);
                    newDist = manhattanSegDis(&data,&medoid);
                    if(newDist < oldDist){
                        oldDist = newDist;
                    }
                    
                }
               
             
            }
            
           Li.push_back(nearPoints(&dataPointsVector, &medoid, oldDist));
            
        }
        
        std::vector< std::vector<int> > Di = findDimensions(medoid_size, l, &Li, &medoids_current); // This returns repeated dimensions
//        std::vector<int> di = Di.at(3);
//        for (int dim : di){
//            std::cout<< dim << '\n';
//        }
//        std::cout << "\n\n";
//        std::vector<int> d2 = Di.at(6);
//        for (int dim : d2){
//            std::cout<< dim << '\n';
//        }
//
        std::cout << "Find Dimension done! \n" ;
        
        std::vector< std::vector< std::vector<double> > > clusters = assignPoints(&Di,&medoids_current);
        int numOfCluster = clusters.size();
        for (int it = 0; it < numOfCluster; it++) {
            std::vector< std::vector<double> > cluster = clusters.at(it);
            std::cout << "Number of points in cluster "<<it<< " : "<<cluster.size()<<'\n';
        }
        for (int clusterNum : output) {
            std::cout<<clusterNum <<" ";
        }
        
       // std::cout << "Would you like to terminate and return the result? " ;
      //  std::cin >> terminate;
   // }
    
    
    
    
}

double manhattanSegDis(std::vector<double> *x1 , std::vector<double> *x2){
    
    std::vector<double> *X1 = x1;
    std::vector<double> *X2 = x2;
    
    int n_dimensions = X1->size();
    if (X1->size() != X2->size()){
        return -1.0;
    }
    else{
        double totalDis = 0.0;
        std::vector<double>::iterator itx1;
        std::vector<double>::iterator itx2;
        for(itx1=X1->begin(), itx2 = X2->begin() ; itx1 < X1->end(); itx1++, itx2++){
            totalDis += std::abs(*itx1 - *itx2);
        }
        return totalDis / n_dimensions;
        
    }
    
    
    
    
    
}

double manhattanSegDisDim(std::vector<double> *x1 , std::vector<double> *x2, int numDim){
    
    std::vector<double> *X1 = x1;
    std::vector<double> *X2 = x2;
    double totalDis;
    
    
    totalDis = std::abs(X1->at(numDim) - X2->at(numDim));
    
    
    return totalDis;
    
}

std::vector< std::vector<double> > randomInitialSample(std::vector< std::vector<double> > *dataPointsVector, int size){
    std::vector< std::vector<double> > *inputDataPoints = dataPointsVector;
    std::vector< std::vector<double> > outputSample;
    int sizeOfDataPoints = inputDataPoints->size();
    if(size < sizeOfDataPoints){
        
        int upperBoundIndex = sizeOfDataPoints - size - 1 ;
        int r = rand() % upperBoundIndex; // rand is not random
        std::cout << std::fixed << r <<'\n';
        int startIndex =  r;
        //std::cout << startIndex << " Start Index" << '\n';
        for(int i = 0 ; i<size ;i++)
            outputSample.push_back(inputDataPoints->at(startIndex + i));
        
        
    }
    else{
        // empty outputSample will be returned .
        //std::cout << "Size is greator than number of dataPoints";
    }
    
    return outputSample;
    
    
}

std::vector< std::vector<double> > greedySample(std::vector< std::vector<double> > *dataPointsVector, int size){
    std::vector< std::vector<double> > *inputDataPoints = dataPointsVector;
    std::vector<double> distance;
    std::vector< std::vector<double> > medoids;
    int random = rand() % inputDataPoints->size();
    std::vector<double> m = inputDataPoints->at(random);
    medoids.push_back(m);
    inputDataPoints->erase(inputDataPoints->begin() + random);
    
    for(std::vector<double> s : *inputDataPoints ){
        distance.push_back(manhattanSegDis(&s,&m));
    }
    int maxEleIndex;
    for (int i = 1 ; i < size; i++ ){
         maxEleIndex = std::distance( distance.begin(), std::max_element(distance.begin(), distance.end()));
        //std::cout << " Farthest element: " << maxEleIndex << '\n';
        m = inputDataPoints->at(maxEleIndex);
        medoids.push_back(m);
        inputDataPoints->erase(inputDataPoints->begin() + maxEleIndex);
        distance.erase(distance.begin() + maxEleIndex);
        std::vector< std::vector<double> >::iterator s_ite;
        std::vector<double>::iterator d_ite;
        int k = 0;
        double newDist;
        for( s_ite=inputDataPoints->begin(), d_ite = distance.begin() ; s_ite < inputDataPoints->end(); s_ite++, d_ite++, k++ ){
            newDist = manhattanSegDis(&*s_ite,&m); // This gives output 0
            if(distance.at(k) > newDist){
                distance.at(k) = newDist;
            }
        }
        
        
    }
    
    return medoids;
    
}



std::vector< std::vector<double> > nearPoints(std::vector< std::vector<double> > *dataPointsVector, std::vector<double> *medoid, double radius){

    std::vector< std::vector<double> > nearestDataPoints ;
    
    double distance;
    for( std::vector<double> data : *dataPointsVector){
        
        distance = manhattanSegDis(&data,medoid);
        double diff = distance - radius;
        if ( diff <= 0.00) {
           // std::cout << "Points added to medoid with radius "<<radius;
            nearestDataPoints.push_back(data);
        }
    }
    
    
    return nearestDataPoints;
}

std::vector<double> avgDist(std::vector< std::vector<double> > *Li,  std::vector<double>  *medoid){
    std::vector<double> avgDistRes ;
    
    int numOfPoints = Li->size();
    int numDim = medoid->size();
    for( int i = 0; i<numDim ; i++){
        double totalDis = 0.0;
        for (std::vector<double> point : *Li) {
            totalDis += manhattanSegDisDim(&point,medoid, i);
        }
        
        avgDistRes.push_back(totalDis/numOfPoints);
    }


    return avgDistRes;
}


std::vector< std::vector<int> > findDimensions(int k, int l,std::vector< std::vector< std::vector<double> > > *L, std::vector< std::vector<double> > *medoids ){
    
    std::vector< std::vector<int> > Di;
    std::vector< std::pair<double, int> > zij;
    int numMeds = medoids->size();
    for (int index = 0; index < numMeds; index++) {
        
        std::vector< std::vector<double> > li = L->at(index);
        std::vector<double> medoid = medoids->at(index);
        
        int numDim = medoid.size();
        std::vector<double> xijs = avgDist(&li, &medoid);
        
        double Yi  = 0.0;
        for (double xij : xijs) {
            Yi += xij;
        }
        
        Yi = Yi/numDim;
        
        double sigi = 0.0;
        double temp = 0.0;
        for (double xij : xijs) {
            temp += ((xij - Yi)*(xij - Yi));
        }
        temp /= (numDim -1);
        sigi = std::sqrt(temp);
        
        for ( int j =0; j<numDim; j++)
        {
            double xij = xijs.at(j);
            double diffe = xij - Yi;
            double z_val = diffe/sigi;
            zij.push_back(std::make_pair(z_val, j));
        }
        
        
        std::sort(zij.begin(), zij.end(), pairCompare);
        
        int pickNum = k*l;
        
        if(pickNum > zij.size() )
            std::cout<< "K * L size missmatch";
        else{
            std::vector<int> di;
            for (int t = 0; t < pickNum; t++) {
                di.push_back(zij.at(t).second);
            }
            Di.push_back(di);
        }
    }
    
    return Di;
}

std::vector<std::vector < std::vector<double> > > assignPoints( std::vector< std::vector<int> > *dimensions, std::vector< std::vector<double> > *medoids){

    std::vector< std::vector< std::vector<double> > > Cis;
    int k  =  dimensions->size();
    Cis.resize(k);
    int dataPointIndex = 0;
    for( std::vector<double> point : dataPointsVector)
    {   double dist = 10000.0; // Intiallizing with large values
        int clustIndex = -1;
        for (int ite =0; ite < k; ite++) {
            std::vector<int> dimension = dimensions->at(ite);
            std::vector<double> medoid = medoids->at(ite);
            double currentDist = manhattanSegDisWithDim(&point,&medoid, &dimension);
            if (dist > currentDist) {
                dist = currentDist;
                clustIndex = ite;
            }
            
        }
        if (clustIndex != -1) {
            // else is outlier
            std::vector<std::vector<double> > ci = Cis.at(clustIndex);
            ci.push_back(point);
            Cis.at(clustIndex) = ci;
            output.at(dataPointIndex) = clustIndex;
            if (dataPointIndex != dataPoints-1) {
                myfile<<clustIndex<<",";
            }
            else{
                myfile<<clustIndex;
            }
            
        }
        std::cout<<"Datapoint "<<dataPointIndex<<" has been clustered!\n";
        dataPointIndex++;
        
        
    
    }
    myfile.close();
    
    return Cis;
}

double manhattanSegDisWithDim( std::vector<double>  *point , std::vector<double>  *medoid, std::vector<int>  *dimension   ){
    double distance = 0.0;
    
    int numDim = dimension->size();
    int dimeIndex ;
    for (int it = 0; it < numDim; it++) {
        dimeIndex = dimension->at(it);
    
        distance += std::abs( point->at(dimeIndex) -  medoid->at(dimeIndex));
        
    }
    return  distance/numDim;

}