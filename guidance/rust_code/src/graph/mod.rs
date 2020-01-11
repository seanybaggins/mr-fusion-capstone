use std::fs::File;
use std::io::{prelude::*, BufReader, Seek, SeekFrom};

#[derive(PartialEq, PartialOrd, Debug)]
pub(self) struct GPSPoint {
    latitude: f64,
    longitude: f64,
}

#[derive(Debug)]
pub struct Edge {
    // For debugging, finding out what line we are looking at on the map
    name: String,
    gps_points: Vec<GPSPoint>,
}

impl Edge {
    //fn distance() -> f64;
}

#[derive(Debug)]
pub struct Vertex {
    // Will be used to display key locations to UI
    name: String,

    // Will be used to identify adjacent nodes and edges
    pub(self) gps_point: GPSPoint,

    // Key data to determine the shortest path using Dijkstra's Algorithm
    parent_vertex: Option<&'static Vertex>,
    tentative_distance: Option<f64>,

    // Vertex that is connected to an edge will have the same GPS coordinate of
    // the first or last GPS coordinate of
    adjacent_vertices: Vec<&'static Vertex>,
    adjacent_edges: Vec<&'static Edge>,
}

impl Vertex {
    fn new(name: String, gps_point: GPSPoint) -> Vertex {
        Vertex {
            name,
            gps_point,
            parent_vertex: None,
            tentative_distance: None,
            adjacent_vertices: Vec::with_capacity(0),
            adjacent_edges: Vec::with_capacity(0),
        }
    }
}

impl PartialEq for Vertex {
    fn eq(&self, other: &Vertex) -> bool {
        if self.name == other.name && self.gps_point == other.gps_point {
            return true;
        }

        false
    }
}

/// Returns (number_of_edges, number_of_vertices) in the Buffer of KML contents
pub(self) fn number_of_edges_and_vertices_from_buffer(reader: &mut BufReader<File>) -> (u32, u32) {
    reader.seek(SeekFrom::Start(0)).unwrap();

    let mut number_of_edges = 0;
    let mut number_of_vertices = 0;

    let mut graph_element_found = false;
    for line in reader.lines() {
        let line = line.unwrap();

        if line.contains("<Placemark>") {
            graph_element_found = true;
        } else if line.contains("</Placemark>") {
            graph_element_found = false;
        }

        if graph_element_found {
            // Then determine whether its edge or vertex
            if line.contains("<name>Line") {
                number_of_edges += 1;
            } else if line.contains("<name>Point") || line.contains("<name>") {
                // Covers case of a regular vertex, or a key point in our graph
                number_of_vertices += 1;
            }
        }
    }

    return (number_of_edges, number_of_vertices);
}

pub(self) fn add_gps_points_to_edges(
    reader: &mut BufReader<File>,
    edges: &mut Vec<Edge>,
) {
    // Rewind and start from beginning of contents of buffer
    reader.seek(SeekFrom::Start(0)).unwrap();

    // buffer for reading lines
    let mut line = String::new();

    let mut edge_found = false;

    // Items needed for Edge
    let mut name = String::new();

    while let Ok(_) = reader.read_line(&mut line) {
        if line == "" {
            // Then we are at the end of the file
            break;
        }

        if line.contains("<name>Line") {
            edge_found = true;

            // remove garbage from xml headers and store name
            name = String::from(
                line.replace("<name>", "")
                .replace("</name>", "")
                .trim()
            );
        }

        if edge_found {

            // Allocating memory for the number of gps points in an edge
            let number_of_gps_points = number_of_gps_points_for_edge(reader);
            let mut gps_points: Vec<GPSPoint> =
                Vec::with_capacity(number_of_gps_points as usize);

            // Populating the GPS Points
            let mut gps_string = String::new();
            reader.read_line(&mut gps_string).unwrap();

            // Fast forward until we have coordinates
            while !gps_string.contains("<coordinates>") {
                gps_string.clear();
                reader.read_line(&mut gps_string).unwrap();
            }

            while {
                gps_string.clear();
                reader.read_line(&mut gps_string).unwrap();
                !gps_string.contains("</coordinates>")
            } {
                let (latitude, longitude) = parse_gps_string(&gps_string);
                let gps_point = GPSPoint {latitude, longitude};
                gps_points.push(gps_point);
            }
;
            edges.push(
                Edge {
                    name: name.clone(), 
                    gps_points
                }
            );
            
            edge_found = false;
        }
        
        line.clear();
    }
}

pub(self) fn add_gps_points_to_vertices(
    reader: &mut BufReader<File>,
    vertices: &mut Vec<Vertex>,
) {
    // Rewind and start from beginning of contents of buffer
    reader.seek(SeekFrom::Start(0)).unwrap();
    let mut line = String::new();

    let mut graph_element_found = false;
    let mut vertex_found = false;
    
    let mut name = String::new();
    
    while let Ok(_) = reader.read_line(&mut line) {

        if line == "" {
            // then we have reached the end of the file
            break;
        }

        if line.contains("<Placemark>") {
            graph_element_found = true;
        }

        if graph_element_found {
            if (line.contains("<name>Point") || line.contains("<name>")) 
            && !line.contains("Line") {
                vertex_found = true;

                // remove garbage xml headers and store name
                name = String::from(
                    line.replace("<name>", "").replace("</name>", "").trim()
                );
            }
        }

        if vertex_found {
            // Populate data for the vertex
           if line.contains("<coordinates>") {
                // Then the next line in the file contains the lat, long data
                // for the vertex
                let mut gps_string = String::new();
                reader.read_line(&mut gps_string).unwrap();

                let (latitude, longitude) = parse_gps_string(&gps_string);

                let gps_point = GPSPoint {
                    latitude,
                    longitude
                };

                let vertex = Vertex::new(
                    name.clone(),
                    gps_point
                );

                vertices.push(vertex);
                vertex_found = false;
           }
        }

        if line.contains("</Placemark>") {
            graph_element_found = false;
        }

        line.clear();
    }
}

/// PreCondition: Must be inside the scope of a <name> before or at 
/// <coordinates>
///
/// PostCondition: The number of points for an edge is returned and the
/// curser is moved to location after the points have been read.
pub(self) fn number_of_gps_points_for_edge(reader: &mut BufReader<File>) -> u32 {
    let start_read_location = reader.seek(SeekFrom::Current(0)).unwrap();
    let mut lines = reader.by_ref().lines();
    let mut number_of_gps_points = 0;

    // Move the curser that over the <coordinates> line
    let mut line = lines.next().unwrap().unwrap();

    if !line.contains("<coordinates>") {
        // Fast forward until line does contain <coordinates>
        while {
            line.clear();
            line = lines.next().unwrap().unwrap();
            !line.contains("<coordinates>")
        } {}
    }

    // Rust do-while loop
    while {
        line.clear();
        line = lines.next().unwrap().unwrap();
        !line.contains("</coordinates>")
    } {
        number_of_gps_points += 1;
    }

    // Set the buffer curser back to where it was before the invocation of this
    // this function
    reader.seek(SeekFrom::Start(start_read_location)).unwrap();

    number_of_gps_points
}

pub(self) fn parse_gps_string(gps_string: &String) -> (f64, f64) {
    let gps_data = gps_string
        .trim() // Remove whitespace
        .split(",") // Remove commas and store remaining information in Vec
        .map(|s| s.parse().unwrap()) // Convert vec of strings to integers
        .collect::<Vec<f64>>();

    let latitude = gps_data[0];
    let longitude = gps_data[1];

    (latitude, longitude)
}

pub(self) fn connect_vertices_with_edges(
    edges: &mut Vec<Edge>, 
    vertices: &mut Vec<Vertex>
) {
    
    for edge in edges.iter() {
        for vertex_a_offset in 0..vertices.len() {
            for vertex_b_offset in 0..vertices.len() {
                let vertices_ptr = vertices.as_mut_ptr();
                unsafe {
                    let vertex_a = vertices_ptr.add(vertex_a_offset);
                    let vertex_b = vertices_ptr.add(vertex_b_offset);
                    let vertices_need_connection = {

                        (edge.gps_points.first() == Some(&(*vertex_a).gps_point) &&
                        edge.gps_points.last() == Some(&(*vertex_b).gps_point))
                        ||
                        (edge.gps_points.first() == Some(&(*vertex_b).gps_point) &&
                        edge.gps_points.last() == Some(&(*vertex_a).gps_point))

                    };

                    if vertices_need_connection {
                        (*vertex_a).adjacent_edges.push(edge);
                    }
                }
            }
        }
    }
}


pub fn initialize_from_kml_file(name: &str) -> (Vec<Edge>, Vec<Vertex>) {
    let file = File::open(name).unwrap();
    // Open the file and store its contents to a buffer in RAM
    let mut reader = BufReader::new(file);

    let (number_of_edges, number_of_vertices) =
        number_of_edges_and_vertices_from_buffer(&mut reader);

    let (mut edges, mut vertices) = (
        Vec::with_capacity(number_of_edges as usize),
        Vec::with_capacity(number_of_vertices as usize),
    );

    add_gps_points_to_edges(&mut reader, &mut edges);
    add_gps_points_to_vertices(&mut reader, &mut vertices);

    connect_vertices_with_edges(&mut edges, &mut vertices);

    (edges, vertices)
}

#[cfg(test)]
mod tests;