/**
 * \file ProximityClusterer.h
 *
 * \ingroup Clusterer
 * 
 * \brief Class def header for a class ProximityClusterer
 *
 * @author david caratelli
 */

/** \addtogroup Clusterer

    @{*/

#ifndef LARLITE_PROXIMITYCLUSTERER_H
#define LARLITE_PROXIMITYCLUSTERER_H

#include "Analysis/ana_base.h"
#include "DataFormat/hit.h"
#include <map>

namespace larlite {
  /**
     \class ProximityClusterer
     User custom analysis class made by SHELL_USER_NAME
   */
  class ProximityClusterer : public ana_base{
  
  public:

    /// Default constructor
    ProximityClusterer();

    /// Default destructor
    virtual ~ProximityClusterer(){}

    /** IMPLEMENT in ProximityClusterer.cc!
        Initialization method to be called before the analysis event loop.
    */ 
    virtual bool initialize();

    /** IMPLEMENT in ProximityClusterer.cc! 
        Analyze a data event-by-event  
    */
    virtual bool analyze(storage_manager* storage);

    /** IMPLEMENT in ProximityClusterer.cc! 
        Finalize method to be called after all events processed.
    */
    virtual bool finalize();

    /// Set the size of each cell for hit-map
    void setCellSize(double d) { _cellSize = d; }
    /// Set the radius around which to search for hits
    /// if two hits are within this distance of each other
    /// then they go into the same cluster
    void setRadius(double d) { _radius = d; }
    /// Set which plane to select hits from
    void setPlane(int pl) { _plane = pl; }
    /// Verbosity setter
    void setVerbose(bool on) { _verbose = on; }
    
    /// Set Producers
    void setHitProducer(std::string s) { _hitProducer = s; }
    void setOutClusProducer(std::string s) { _out_clusterProducer = s; }
    
    /// set min tick
    void setMinTick(int t) { _tick_min = t; }
    void setMaxTick(int t) { _tick_max = t; }

  protected:

    /// size of each cell [cm]
    double _cellSize;

    /// radius to count charge around [cm]
    double _radius;
    
    /// plane to select hits from
    int _plane;

    /// verbosity flag
    bool _verbose;

    /// conversion factors for hits
    double _wire2cm, _time2cm;

    /// Producers
    std::string _hitProducer;
    std::string _vtxProducer;
    std::string _out_clusterProducer;

    /// Map making function
    void MakeHitMap(const event_hit* hitlist, int plane);

    /// Functions to decide if two hits should belong to the same cluster or not
    bool HitsCompatible(const hit& h1, const hit& h2);

    /// Function to get neighboring hits (from self + neighoring cells)
    void getNeighboringHits(const std::pair<int,int>& pair, std::vector<size_t>& hitIndices);

    /// check if time overlaps
    double TimeOverlap(const larlite::hit& h1, const larlite::hit& h2) const;

    /// map connecting coordinate index (i,j) to [h1,h2,h3] (hit index list)
    std::map<std::pair<int,int>, std::vector<size_t> > _hitMap;

    /// maximum i'th and j'th
    int _maxI;
    int _maxJ;

    // min and max ticks allowed
    int _tick_min, _tick_max;

    
  };
}
#endif

//**************************************************************************
// 
// For Analysis framework documentation, read Manual.pdf here:
//
// http://microboone-docdb.fnal.gov:8080/cgi-bin/ShowDocument?docid=3183
//
//**************************************************************************

/** @} */ // end of doxygen group 
