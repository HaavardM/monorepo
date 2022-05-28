use chrono::Local;
use google_cloud::pubsub;
use serde::Serialize;
use std::sync::{Arc, Mutex};
use warp::Filter;

#[derive(Clone, Serialize, Debug)]
struct State {
    value: String,
    timestamp: String,
}

#[derive(Clone, Serialize, Debug)]
struct Response {
    current: State,
    history: Vec<State>,
}

impl Default for Response {
    fn default() -> Self {
        Response {
            current: State {
                value: "".to_string(),
                timestamp: "".to_string(),
            },
            history: Vec::<State>::new(),
        }
    }
}

async fn run_pubsub(response: Arc<Mutex<Response>>) {
    let mut client = match pubsub::Client::new("wearebrews").await {
        Ok(c) => c,
        Err(e) => panic!("Unable to create pubsub client: {:?}", e),
    };

    let sub = match client.subscription("anyonethere").await {
        Ok(s) => s,
        Err(e) => panic!("Unable to load subscription: {:?}", e),
    };
    let mut sub: pubsub::Subscription = match sub {
        Some(s) => s,
        None => panic!("No subscription found"),
    };
    loop {
        std::thread::sleep(std::time::Duration::from_millis(1000));
        let mut received = match sub.receive().await {
            Some(m) => m,
            None => panic!("Unable to receive message"),
        };
        let date = Local::now().format("%Y-%m-%d %H:%M:%S");
        let attr: &std::collections::HashMap<String, String> = received.attributes();
        if Some(&"pir".to_string()) == attr.get("subFolder") {
            let msg = match String::from_utf8(received.data().to_owned()) {
                Ok(s) => State {
                    value: s,
                    timestamp: Local::now().to_rfc3339_opts(chrono::SecondsFormat::Millis, true),
                },
                Err(e) => panic!("unable to decode message: {:?}", e),
            };
            let mut resp = response.lock().unwrap();

            if resp.history.len() >= 20 {
                resp.history.remove(0);
            }
            resp.history.push(msg.clone());
            resp.current = msg;
            println!(
                "Accepting message {} at {}",
                std::str::from_utf8(received.data()).expect("unable to decode utf8 string"),
                date
            );
        } else {
            println!(
                "Dropping message from subFolder {} at {}",
                attr.get("subFolder").unwrap_or(&String::from("NONE")),
                date
            )
        }
        received.ack().await.expect("Unable to ack message");
    }
}
async fn run_http(response: Arc<Mutex<Response>>) {
    let cors = warp::cors()
        .allow_origin("https://mellbye.dev")
        .allow_origin("http://localhost:8080")
        .allow_methods(vec!["GET"]);

    let state_handler = warp::path::end()
        .map(move || warp::reply::json(&*response.lock().unwrap()))
        .with(cors);
    println!("Starting service");
    warp::serve(state_handler).run(([0, 0, 0, 0], 3030)).await;
}

#[tokio::main]
async fn main() {
    let state_http = Arc::new(Mutex::new(Response::default()));
    let state_pubsub = state_http.clone();
    let t1 = run_pubsub(state_pubsub);
    let t2 = run_http(state_http);
    futures::join!(t1, t2);
}
