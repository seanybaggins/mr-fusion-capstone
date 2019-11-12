use std::fs::File;
use std::io::{prelude::*, BufReader, Seek, SeekFrom};
use std::mem::size_of;
mod tests;

struct GPSPoint {
    latitude: u32,
    longitude: u32,
}

pub struct Edge {
    // For debugging, finding out what line we are looking at on the map
    map_name: String,
    gps_points: Vec<GPSPoint>,
}

impl Edge {
    //fn distance() -> f64;
}

pub struct Vertex {
    // Will be used to display key locations to UI
    name: String,

    // Will be used to identify adjacent nodes and edges
    gps_point: GPSPoint,

    // Key data to determine the shortest path using Dijkstra's Algorithm
    parent_vertex: &'static Vertex,
    tentative_distance: f64,

    // Vertex that is connected to an edge will have the same GPS coordinate of
    // the first or last GPS coordinate of 
    adjacent_vertices: Vec<& 'static Vertex>,
    adjacent_edges: Vec<&'static Edge>,
}

/// Returns (number_of_edges, number_of_vertices) in the Buffer of KML contents
pub(self) fn number_of_edges_and_vertices_from_buffer
(reader: &mut BufReader<File>) -> (u32, u32) {
    reader.seek(SeekFrom::Start(0)).unwrap();

    let mut number_of_edges = 0;
    let mut number_of_vertices = 0; 

    let mut graph_element_found = false;
    
    for line in reader.lines() {
        let line = line.unwrap();

        if line.contains("<Placemark>") {
            graph_element_found = true;
        }
        else if line.contains("</Placemark>") {
            graph_element_found = false;
        }

        if graph_element_found {
            // Then determine whether its edge or vertex
            if line.contains("<name>Line") {
                number_of_edges += 1;
            }
            else if line.contains("<name>Point") || line.contains("<name>") {
                // Covers case of a regular vertex, or a key point in our graph
                number_of_vertices += 1;
            }
        }
    }

    return (number_of_edges, number_of_vertices);                                                
}

pub(self) fn add_gps_points_to_graph(reader: &mut BufReader<File>, 
                                     edges: &mut Vec<Edge>, 
                                     vertices: &mut Vec<Vertex>) {
    // Rewind and start from beginning of contents of buffer
    reader.seek(SeekFrom::Start(0));

    let mut lines = reader.lines();

    let mut graph_element_found = false;
    let mut vertex_found = false;
    let mut edge_found = false;
    
    while let Some(Ok(line)) = lines.next() {

        if line.contains("<Placemark>") {
            graph_element_found = true;
        }

        if graph_element_found {
            if line.contains("<name>Line") {
                edge_found = true;
            }
            else if line.contains("<name>Point") || line.contains("<name>") {
                vertex_found = true;
            }
        }

        if edge_found {
            number_of_gps_points_for_edge(reader);
        }

        if line.contains("</Placemark>") {
            graph_element_found = false;
            edge_found = false;
            vertex_found = false;
        }
        
    }
}

/// PreCondition: Curser in the reader must be pointing to the 
/// <coordinates> section of the KML file
/// 
/// PostCondition: The number of points for an edge is returned and the 
/// curser is moved to location after the points have been read.
pub(self) fn number_of_gps_points_for_edge(reader: &mut BufReader<File>) -> u32 {
    
    let start_read_location = reader.seek(SeekFrom::Current(0)).unwrap();
    let mut lines = reader.by_ref().lines();
    let mut number_of_gps_points = 0;

    // Check the precondition and panic if not met
    let mut line = lines.next().unwrap().unwrap();

    // Rust do-while loop
    while {
        number_of_gps_points += 1;
        println!("{:?}", line);
        line = lines.next().unwrap().unwrap(); 
        !line.contains("</coordinates>")
    } {}

    // Set the buffer curser back to where it was before the invocation of this
    // this function
    reader.seek(SeekFrom::Start(start_read_location)).unwrap();

    return number_of_gps_points;

}

pub fn initialize_from_kml_file(name: &str) -> 
(Vec<Edge>, Vec<Vertex>) {
    let file = File::open(name).unwrap();
    
    // Open the file and store its contents to a buffer in RAM
    let mut reader = BufReader::new(file);

    let (number_of_edges, number_of_vertices) = 
        number_of_edges_and_vertices_from_buffer(&mut reader);


    let (mut edges,
         mut vertices) = (
                            Vec::with_capacity(
                                size_of::<Edge>() * number_of_edges as usize
                            ),
                            Vec::with_capacity(
                                size_of::<Vertex>() * number_of_vertices as usize
                            )
                        );

    add_gps_points_to_graph(&mut reader, &mut edges, &mut vertices);

    return (edges, vertices)
}