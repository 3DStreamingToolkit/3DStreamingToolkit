using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using Windows.Data.Json;

namespace PeerConnectionClient.Signalling
{
	/// <summary>
	/// A temporary turn credential client
	/// </summary>
	public class TemporaryTurnClient : IDisposable
	{
		/// <summary>
		/// Represents the credentials provided by a turn credential provider service
		/// </summary>
		public struct TurnCredentials
		{
			/// <summary>
			/// The underlying http status of the response
			/// </summary>
			public int http_status;

			/// <summary>
			/// The username provided in the response
			/// </summary>
			public string username;

			/// <summary>
			/// The password provided in the response
			/// </summary>
			public string password;
		}

		/// <summary>
		/// Event delegate for when credentials are retrieved
		/// </summary>
		/// <param name="creds">the credentials</param>
		public delegate void OnCredentialsRetrieved(TurnCredentials creds);

		/// <summary>
		/// Fired when credentials are retrieved
		/// </summary>
		/// <remarks>
		/// Use <see cref="TurnCredentials.http_status"/> to determine success or failure
		/// </remarks>
		public event OnCredentialsRetrieved CredentialsRetrieved;

		/// <summary>
		/// Internal representation of the turn credential provider service uri
		/// </summary>
		private string turnCredentialUri;

		/// <summary>
		/// Internal representation of the http client used to communicate with the turn credential provider service
		/// </summary>
		private HttpClient client;

		/// <summary>
		/// Internally indicates if the object has been disposed
		/// </summary>
		/// <remarks>
		/// This is part of the <see cref="IDisposable"/> pattern
		/// </remarks>
		private bool disposedValue;
		
		/// <summary>
		/// Default ctor
		/// </summary>
		/// <param name="turnCredentialUri">the turn credential provider service uri</param>
		public TemporaryTurnClient(string turnCredentialUri)
		{
			if (!Uri.IsWellFormedUriString(turnCredentialUri, UriKind.Absolute))
			{
				throw new ArgumentException(nameof(turnCredentialUri));
			}

			this.turnCredentialUri = turnCredentialUri;
			this.client = new HttpClient();
			this.disposedValue = false;
		}

		/// <summary>
		/// Triggers the turn credential retrieval flow
		/// </summary>
		/// <remarks>
		/// This returns a boolean to remain compatible with our native
		/// implementation - however it always returns true in managed code
		/// </remarks>
		/// <param name="authHeader">the value of the authentication header to use</param>
		/// <returns><c>true</c></returns>
		public bool RequestCredentials(AuthenticationHeaderValue authHeader = null)
		{
			if (authHeader != null)
			{
				this.client.DefaultRequestHeaders.Authorization = authHeader;
			}

			this.client.GetAsync(this.turnCredentialUri).ContinueWith(async (Task<HttpResponseMessage> prev) =>
			{
				var message = prev.Result;

				var eventData = new TurnCredentials()
				{
					http_status = (int)message.StatusCode
				};

				if (message.StatusCode == HttpStatusCode.OK)
				{
					var body = await message.Content.ReadAsStringAsync();
					var obj = JsonObject.Parse(body);

					eventData.username = obj["username"].GetString();
					eventData.password = obj["password"].GetString();
				}

				this.CredentialsRetrieved?.Invoke(eventData);
			});

			// this maintains api compatibility with our native version
			// but is technically useless here (since it's always true)
			return true;
		}

		/// <summary>
		/// Implements the <see cref="IDisposable"/> pattern
		/// </summary>
		/// <param name="disposing">indicates if we're currently disposing the object</param>
		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					this.client?.Dispose();
				}

				disposedValue = true;
			}
		}

		/// <summary>
		/// Dispose of the object
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
		}
	}
}
