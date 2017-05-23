#ifndef LARLITE_REMOVEDELTARAYS_CXX
#define LARLITE_REMOVEDELTARAYS_CXX

#include "RemoveDeltaRays.h"

namespace larlite {

  RemoveDeltaRays::RemoveDeltaRays()
    : HitRemovalBase()
  {

    _name        = "RemoveDeltaRays";
    _clusProducer = "";
    _vertexProducer = "";

    _d_delta_min = _d_delta_max = 0.;
    
  }

  bool RemoveDeltaRays::initialize() {

    return true;
  }
  
  bool RemoveDeltaRays::analyze(storage_manager* storage) {

    _event_watch.Start();

    _clus_bbox.clear();

    auto ev_clus = storage->get_data<event_cluster>(_clusProducer);
    auto ev_vtx  = storage->get_data<event_vertex> (_vertexProducer);

    _ev_hit = nullptr;
    auto const& ass_cluster_hit_v = storage->find_one_ass(ev_clus->id(), _ev_hit, ev_clus->name());

    if (!_ev_hit){
      print(larlite::msg::kWARNING,__FUNCTION__,"no hits");
      return false;
    }

    if (loadVertex(ev_vtx) == false) {
      print(larlite::msg::kERROR,__FUNCTION__,"num. vertices != 1");
      return false;
    }

    // keep track of all clusters identified as delta-rays
    std::vector<size_t> delta_ray_v;
    
    // loop through clusters and identify those removed
    // by cosmic removal, per plane
    std::vector< std::vector<size_t> > cosmic_clus_v(3,std::vector<size_t>());

    for (size_t i=0; i < ass_cluster_hit_v.size(); i++) {

      auto hit_idx_v = ass_cluster_hit_v[i];
      
      int pl = _ev_hit->at(hit_idx_v[0]).WireID().Plane;

      // only consider as cosmics the tracks with more hits than the max allowed for a delta-ray
      if (hit_idx_v.size() < _max_delta_hits) continue;

      // if negative GoF -> removed.
      if ( _ev_hit->at(hit_idx_v.at(0)).GoodnessOfFit() < 0 ) {

	cosmic_clus_v[pl].push_back( i );

	// determine cluster's bbox
	double wmin, tmin, wmax, tmax;
	wmin = tmin = 10000.;
	wmax = tmax = 0.;
	
	for (auto const& hit_idx : hit_idx_v){
	  double w = _ev_hit->at(hit_idx).WireID().Wire  * _wire2cm;
	  double t = _ev_hit->at(hit_idx).PeakTime() * _time2cm;
	  if (w > wmax) wmax = w;
	  if (w < wmin) wmin = w;
	  if (t > tmax) tmax = t;
	  if (t < tmin) tmin = t;
	}// for all hits

	BBox box;
	box.wmin = wmin;
	box.wmax = wmax;
	box.tmin = tmin;
	box.tmax = tmax;
	_clus_bbox[ i ] = box;
	
      }// if cluster was removed

    }// for all clusters

    if (_verbose) std::cout << "there are " << cosmic_clus_v[2].size() << " cosmic muons on plane 2" << std::endl;
    
    // now for small clusters that have not been removed
    // identify if they are close to a removed cosmic
    // cluster and delta-ray like

    for (size_t i=0; i < ass_cluster_hit_v.size(); i++) {
      
      auto hit_idx_v = ass_cluster_hit_v[i];

      int pl = _ev_hit->at(hit_idx_v[0]).WireID().Plane;

      if (hit_idx_v.size() == 0) continue;

      // if negative GoF -> removed, ignore
      bool already_removed = true;
      for (auto const& hit_idx : hit_idx_v) {
	if (_ev_hit->at(hit_idx).GoodnessOfFit() > 0 ) {
	  already_removed = false;
	  break;
	}
      }

      if (already_removed == true) continue;
      
      // if too many hits -> ignore
      if (hit_idx_v.size() > _max_delta_hits) continue;

      /*
      // if too few hits -> remove
      if (hit_idx_v.size() <= 10) {
	delta_ray_v.push_back( i );
	continue;
      }
      */

      // if out of ROI, ignore this delta-ray (OK not to remove)
      auto const& hit0 = _ev_hit->at(hit_idx_v.at(0));
      
      double wcm = hit0.WireID().Wire * _wire2cm ;
      double tcm = hit0.PeakTime() * _time2cm;
      double dvtx = sqrt( (wcm - _vtx_w_cm[pl]) * (wcm - _vtx_w_cm[pl]) +
			  (tcm - _vtx_t_cm[pl]) * (tcm - _vtx_t_cm[pl]) );
      
      if (dvtx > _roi) continue;
      
      // compare this delta-ray to all removed muons in the plane
      for (auto const& muidx : cosmic_clus_v[pl]){
	// 1st check if delta-ray compatible with muon's BBox
	auto bbox = _clus_bbox[muidx];
	if (wcm > bbox.wmax + _d_delta_max) continue;
	if (wcm < bbox.wmin - _d_delta_max) continue;
	if (tcm > bbox.tmax + _d_delta_max) continue;
	if (tcm < bbox.tmin - _d_delta_max) continue;
	if (DeltaRay( ass_cluster_hit_v[muidx], hit_idx_v ) == true )
	delta_ray_v.push_back( i );
      }
      
    }// for all clusters

    for (auto const& delta_ray : delta_ray_v){
      auto hit_idx_v = ass_cluster_hit_v[delta_ray];
      for (auto const& hit_idx : hit_idx_v)
	_ev_hit->at(hit_idx).set_goodness(-1.0);
    }// for all delta-rays

    _event_time += _event_watch.RealTime();
    _event_num  += 1;
      
    return true;
  }

  // input vectors are the indices of the hits of the muon and delta-ray respectively
  bool RemoveDeltaRays::DeltaRay(const std::vector<unsigned int>& muon,
				 const std::vector<unsigned int>& deltaray) {

    // min dist to muon
    // idx of muon hit with min dist
    double ddmin = 1000000.;
    size_t hidxmin;
    // max distance to muon
    double ddmax = 0.0;

    if (_verbose) {
      std::cout << muon.size() << " muon hits" << std::endl;
      std::cout << deltaray.size() << " delta-ray hits" << std::endl;
      std::cout << "delta-ray @ [" << _ev_hit->at(deltaray[0]).PeakTime() * _time2cm << ", " << _ev_hit->at(deltaray[0]).WireID().Wire * _wire2cm << "]" << std::endl;
    }
    
    // find minimum distance between delta-ray candidate and cosmic muon
    for (auto mu_h_idx : muon) {
      for (auto dr_h_idx : deltaray) {

	auto mu_h = _ev_hit->at(mu_h_idx);
	auto dr_h = _ev_hit->at(dr_h_idx);

	double dd = _distSq_(mu_h,dr_h);

	if (dd < ddmin) { ddmin = dd; hidxmin = mu_h_idx; }
      }
    }

    if (_verbose) std::cout << "ddmin is " << sqrt(ddmin) << std::endl;

    if (  sqrt(ddmin) > _d_delta_min ) return false;

    // find max distance to this point
    auto const& mu_h_min = _ev_hit->at(hidxmin);
    
    for (auto const& dr_h_idx : deltaray) {
      
      auto const& dr_h = _ev_hit->at(dr_h_idx);
      
      double dd = _distSq_(dr_h, mu_h_min);

      if (dd > ddmax) { ddmax = dd; }

    }// for all delta-ray hits

    if (_verbose) std::cout << "ddmax is " << sqrt(ddmax) << std::endl;

    if ( sqrt(ddmax) > _d_delta_max ) return false;

    if (_verbose) { std::cout << "\t\t REMOVE DELTA-RAY" << std::endl; }
	
    return true;
  }


  double RemoveDeltaRays::_distSq_(const larlite::hit& h1, const larlite::hit& h2) {

    double t1 = h1.PeakTime() * _time2cm;
    double w1 = h1.WireID().Wire * _wire2cm;
    double t2 = h2.PeakTime() * _time2cm;
    double w2 = h2.WireID().Wire * _wire2cm;
    
    return (t1-t2)*(t1-t2) + (w1-w2)*(w1-w2);
  }


}
#endif
