/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <sstream>

#include "particle_filter.h"

#include "helper_functions.h"

using namespace std;

using std::string;
using std::vector;
using std::cout;

static default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	/**
	 * TODO: Set the number of particles. Initialize all particles to
	 *   first position (based on estimates of x, y, theta and their uncertainties
	 *   from GPS) and all weights to 1.
	 * TODO: Add random Gaussian noise to each particle.
	 * NOTE: Consult particle_filter.h for more information about this method
	 *   (and others in this file).
	 */
	num_particles = 10;

	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);

	for (size_t i = 0; i < num_particles; ++i) {

		//a new particle
		Particle p;
		p.id = int(i);
		p.weight = 1.0;
		p.x = dist_x(gen);
		p.y = dist_y(gen);
		p.theta = dist_theta(gen);

		particles.push_back(p);
	}

	is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[],
		double velocity, double yaw_rate) {
	/**
	 * TODO: Add measurements to each particle and add random Gaussian noise.
	 * NOTE: When adding noise you may find std::normal_distribution
	 *   and std::default_random_engine useful.
	 *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	 *  http://www.cplusplus.com/reference/random/default_random_engine/
	 */

	for (size_t i = 0; i < num_particles; ++i) {

		// Gather old values
		double x_old = particles[i].x;
		double y_old = particles[i].y;
		double theta_old = particles[i].theta;

		double theta_pred, x_pred, y_pred;

		if (abs(yaw_rate) > 1e-5) {
			// Apply equations of motion model (turning)
			theta_pred = theta_old + yaw_rate * delta_t;
			x_pred = x_old
					+ velocity / yaw_rate * (sin(theta_pred) - sin(theta_old));
			y_pred = y_old
					+ velocity / yaw_rate * (cos(theta_old) - cos(theta_pred));
		} else {
			// Apply equations of motion model (going straight)
			theta_pred = theta_old;
			x_pred = x_old + velocity * delta_t * cos(theta_old);
			y_pred = y_old + velocity * delta_t * sin(theta_old);
		}

		normal_distribution<double> dist_x(x_pred, std_pos[0]);
		normal_distribution<double> dist_y(y_pred, std_pos[1]);
		normal_distribution<double> dist_theta(theta_pred, std_pos[2]);

		particles[i].x = dist_x(gen);
		particles[i].y = dist_y(gen);
		particles[i].theta = dist_theta(gen);
	}

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted,
		vector<LandmarkObs>& observations) {
	/**
	 * TODO: Find the predicted measurement that is closest to each
	 *   observed measurement and assign the observed measurement to this
	 *   particular landmark.
	 * NOTE: this method will NOT be called by the grading code. But you will
	 *   probably find it useful to implement this method and use it as a helper
	 *   during the updateWeights phase.
	 */

	for (auto& obs : observations) {
		double min_dist = numeric_limits<double>::max();

		for (const auto& pred_obs : predicted) {
			double d = dist(obs.x, obs.y, pred_obs.x, pred_obs.y);
			if (d < min_dist) {
				obs.id = pred_obs.id;
				min_dist = d;
			}
		}
	}

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
		const vector<LandmarkObs> &observations, const Map &map_landmarks) {
	/**
	 * TODO: Update the weights of each particle using a mult-variate Gaussian
	 *   distribution. You can read more about this distribution here:
	 *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	 * NOTE: The observations are given in the VEHICLE'S coordinate system.
	 *   Your particles are located according to the MAP'S coordinate system.
	 *   You will need to transform between the two systems. Keep in mind that
	 *   this transformation requires both rotation AND translation (but no scaling).
	 *   The following is a good resource for the theory:
	 *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	 *   and the following is a good resource for the actual equation to implement
	 *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
	 */

	double std_x = std_landmark[0];
	double std_y = std_landmark[1];

	for (size_t i = 0; i < num_particles; ++i) {

		double p_x = particles[i].x;
		double p_y = particles[i].y;
		double p_theta = particles[i].theta;

		// List all landmarks within sensor range
		vector<LandmarkObs> predicted_landmarks;

		for (const auto& map_landmark : map_landmarks.landmark_list) {
			int l_id = map_landmark.id_i;
			double l_x = (double) map_landmark.x_f;
			double l_y = (double) map_landmark.y_f;

			double d = dist(p_x, p_y, l_x, l_y);
			if (d < sensor_range) {
				LandmarkObs l_pred;
				l_pred.id = l_id;
				l_pred.x = l_x;
				l_pred.y = l_y;
				predicted_landmarks.push_back(l_pred);
			}
		}

		// List all observations in map coordinates
		vector<LandmarkObs> observed_landmarks_map_ref;
		for (size_t j = 0; j < observations.size(); ++j) {

			// Convert observation from particle(vehicle) to map coordinate system
			LandmarkObs rototranslated_obs;
			rototranslated_obs.x = cos(p_theta) * observations[j].x
					- sin(p_theta) * observations[j].y + p_x;
			rototranslated_obs.y = sin(p_theta) * observations[j].x
					+ cos(p_theta) * observations[j].y + p_y;

			observed_landmarks_map_ref.push_back(rototranslated_obs);
		}

		dataAssociation(predicted_landmarks, observed_landmarks_map_ref);

		double particle_likelihood = 1.0;

		double mu_x, mu_y;
		for (const auto& obs : observed_landmarks_map_ref) {

			for (const auto& land : predicted_landmarks)
				if (obs.id == land.id) {
					mu_x = land.x;
					mu_y = land.y;
					break;
				}

			double norm_factor = 2 * M_PI * std_x * std_y;
			double prob = exp(
					-(pow(obs.x - mu_x, 2) / (2 * std_x * std_x)
							+ pow(obs.y - mu_y, 2) / (2 * std_y * std_y)));

			particle_likelihood *= prob / norm_factor;
		}

		particles[i].weight = particle_likelihood;

	}
	double norm_factor = 0.0;
	for (const auto& particle : particles)
		norm_factor += particle.weight;

	// Normalize weights s.t. they sum to one
	for (auto& particle : particles)
		particle.weight /= (norm_factor + numeric_limits<double>::epsilon());

}

void ParticleFilter::resample() {
	/**
	 * TODO: Resample particles with replacement with probability proportional
	 *   to their weight.
	 * NOTE: You may find std::discrete_distribution helpful here.
	 *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
	 */

	vector<double> particle_weights;
	for (const auto& particle : particles)
		particle_weights.push_back(particle.weight);

	discrete_distribution<int> weighted_distribution(particle_weights.begin(),
			particle_weights.end());

	vector<Particle> resampled_particles;
	for (size_t i = 0; i < num_particles; ++i) {
		int k = weighted_distribution(gen);
		resampled_particles.push_back(particles[k]);
	}

	particles = resampled_particles;

	for (auto& particle : particles)
		particle.weight = 1.0;

}

void ParticleFilter::SetAssociations(Particle& particle,
		const vector<int>& associations, const vector<double>& sense_x,
		const vector<double>& sense_y) {
	// particle: the particle to which assign each listed association,
	//   and association's (x,y) world coordinates mapping
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates
	particle.associations = associations;
	particle.sense_x = sense_x;
	particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
	vector<int> v = best.associations;
	std::stringstream ss;
	copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length() - 1);  // get rid of the trailing space
	return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
	vector<double> v;

	if (coord == "X") {
		v = best.sense_x;
	} else {
		v = best.sense_y;
	}

	std::stringstream ss;
	copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
	string s = ss.str();
	s = s.substr(0, s.length() - 1);  // get rid of the trailing space
	return s;
}
