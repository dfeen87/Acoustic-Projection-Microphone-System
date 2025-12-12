docker:
  runs-on: ubuntu-22.04

  steps:
    - name: Checkout code
      uses: actions/checkout@v4

    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v3

    - name: Login to Docker Hub
      if: github.event_name == 'release'
      uses: docker/login-action@v3
      with:
        username: dfeen87
        password: ${{ secrets.DOCKER_PASSWORD }}

    - name: Build Docker image
      uses: docker/build-push-action@v6
      with:
        context: .
        push: ${{ github.event_name == 'release' }}
        tags: |
          dfeen87/apm-system:latest
          dfeen87/apm-system:${{ github.ref_name }}
        cache-from: type=gha
        cache-to: type=gha,mode=max
