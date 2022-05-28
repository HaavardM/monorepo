FROM rust:1.51-alpine AS build

ARG JOBS=1 

WORKDIR /usr/src/api
RUN apk add --no-cache musl-dev protoc
COPY Cargo.* .
RUN rustup component add rustfmt
RUN mkdir src && echo "fn main() { eprintln!(\"Docker build error\")}" > src/main.rs
RUN cargo build --release -j ${JOBS}
RUN rm -rf src target


COPY src src
RUN cargo build --release -j ${JOBS}

FROM alpine
RUN apk add --update --no-cache ca-certificates
COPY --from=build /usr/src/api/target/release/api /usr/local/bin/api
CMD ["api"]