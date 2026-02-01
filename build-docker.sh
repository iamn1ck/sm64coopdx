# run this to install dependecies and everything to setup since docker file doesnt work because of network issues
# docker exec -u 1000 -i ${container_id} bash < init-docker-container.sh 

docker exec -it sm64coopdx-build-env bash -c "cd sm64coopdx && make -j12 TARGET_ANDROID=1 OPENXR=1"